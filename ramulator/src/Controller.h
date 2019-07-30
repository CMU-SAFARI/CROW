#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#include <cassert>
#include <cstdio>
#include <deque>
#include <fstream>
#include <list>
#include <string>
#include <vector>

#include "Config.h"
#include "DRAM.h"
#include "Refresh.h"
#include "Request.h"
#include "Scheduler.h"
#include "Statistics.h"

#include "ALDRAM.h"
#include "SALP.h"
//#include "TLDRAM.h"

#include "CROWTable.h"

using namespace std;

namespace ramulator
{

    extern bool warmup_complete;

template <typename T>
class Controller
{
protected:
    // For counting bandwidth
    ScalarStat read_transaction_bytes;
    ScalarStat write_transaction_bytes;

    ScalarStat row_hits;
    ScalarStat row_misses;
    ScalarStat row_conflicts;
    VectorStat read_row_hits;
    VectorStat read_row_misses;
    VectorStat read_row_conflicts;
    VectorStat write_row_hits;
    VectorStat write_row_misses;
    VectorStat write_row_conflicts;
    ScalarStat useless_activates;

    ScalarStat read_latency_avg;
    ScalarStat read_latency_sum;
    VectorStat read_latency_sum_per_core;
    VectorStat read_latency_avg_per_core;

    ScalarStat req_queue_length_avg;
    ScalarStat req_queue_length_sum;
    ScalarStat read_req_queue_length_avg;
    ScalarStat read_req_queue_length_sum;
    ScalarStat write_req_queue_length_avg;
    ScalarStat write_req_queue_length_sum;

    // CROW
    ScalarStat crow_invPRE;
    ScalarStat crow_invACT;
    ScalarStat crow_full_restore;
    ScalarStat crow_skip_full_restore;
    ScalarStat crow_num_hits;
    ScalarStat crow_num_all_hits;
    ScalarStat crow_num_misses;
    ScalarStat crow_num_copies;

    ScalarStat crow_num_fr_set;
    ScalarStat crow_num_fr_notset;
    ScalarStat crow_num_fr_ref;
    ScalarStat crow_num_fr_restore;
    ScalarStat crow_num_hits_with_fr;
    ScalarStat crow_bypass_copying;

    ScalarStat crow_idle_cycle_due_trcd;
    ScalarStat crow_idle_cycle_due_tras;

    ScalarStat tl_dram_invalidate_due_to_write;
    ScalarStat tl_dram_precharge_cached_row_due_to_write;
    ScalarStat tl_dram_precharge_failed_due_to_timing;
    // END - CROW

#ifndef INTEGRATED_WITH_GEM5
    VectorStat record_read_hits;
    VectorStat record_read_misses;
    VectorStat record_read_conflicts;
    VectorStat record_write_hits;
    VectorStat record_write_misses;
    VectorStat record_write_conflicts;
    VectorStat record_read_latency_avg_per_core;
#endif

public:
    /* Member Variables */
    long clk = 0;
    DRAM<T>* channel;

    Scheduler<T>* scheduler;  // determines the highest priority request whose commands will be issued
    RowPolicy<T>* rowpolicy;  // determines the row-policy (e.g., closed-row vs. open-row)
    RowTable<T>* rowtable;  // tracks metadata about rows (e.g., which are open and for how long)
    Refresh<T>* refresh;

    struct Queue {
        list<Request> q;
        unsigned int max = 64;
        unsigned int size() const {return q.size();}
    };

    Queue readq;  // queue for read requests
    Queue writeq;  // queue for write requests
    Queue actq; // read and write requests for which activate was issued are moved to 
                   // actq, which has higher priority than readq and writeq.
                   // This is an optimization
                   // for avoiding useless activations (i.e., PRECHARGE
                   // after ACTIVATE w/o READ of WRITE command)
    Queue otherq;  // queue for all "other" requests (e.g., refresh)

    deque<Request> pending;  // read requests that are about to receive data from DRAM
    bool write_mode = false;  // whether write requests should be prioritized over reads
    //long refreshed = 0;  // last time refresh requests were generated

    /* Command trace for DRAMPower 3.1 */
    string cmd_trace_prefix = "cmd-trace-";
    vector<ofstream> cmd_trace_files;
    bool record_cmd_trace = false;
    /* Commands to stdout */
    bool print_cmd_trace = false;

    bool enable_crow = false;
    bool enable_crow_upperbound = false;
    bool enable_tl_dram = false;
    int crow_evict_threshold = 0;
    int crow_half_life = 0;
    float crow_to_mru_frac = 0.0f;
    uint crow_table_grouped_SAs = 1;
    uint copy_rows_per_SA = 0;
    uint weak_rows_per_SA = 0;
    float refresh_mult = 1.0f;
    bool prioritize_evict_fully_restored = false;
    bool collect_row_act_histogram = false;
    int num_SAs = 0;

    bool is_DDR4 = false, is_LPDDR4 = false; // Hasan

    CROWTable<T>* crow_table = nullptr;
    int* ref_counters;
    
    bool refresh_disabled = false;

    /* Constructor */
    Controller(const Config& configs, DRAM<T>* channel) :
        channel(channel),
        scheduler(new Scheduler<T>(this, configs)),
        rowpolicy(new RowPolicy<T>(this, configs)),
        rowtable(new RowTable<T>(this)),
        refresh(new Refresh<T>(this)),
        cmd_trace_files(channel->children.size())
    {
        record_cmd_trace = configs.get_bool("record_cmd_trace");
        print_cmd_trace = configs.get_bool("print_cmd_trace");
        if (record_cmd_trace){
            if (configs["cmd_trace_prefix"] != "") {
              cmd_trace_prefix = configs["cmd_trace_prefix"];
            }
            string prefix = cmd_trace_prefix + "chan-" + to_string(channel->id) + "-rank-";
            string suffix = ".cmdtrace";
            for (unsigned int i = 0; i < channel->children.size(); i++)
                cmd_trace_files[i].open(prefix + to_string(i) + suffix);
        }

        num_SAs = configs.get_int("subarrays");


        reload_options(configs);

        is_DDR4 = channel->spec->standard_name == "DDR4";
        is_LPDDR4 = channel->spec->standard_name == "LPDDR4";


        // regStats

        row_hits
            .name("row_hits_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row hits per channel per core")
            .precision(0)
            ;
        row_misses
            .name("row_misses_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row misses per channel per core")
            .precision(0)
            ;
        row_conflicts
            .name("row_conflicts_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row conflicts per channel per core")
            .precision(0)
            ;

        read_row_hits
            .init(configs.get_int("cores"))
            .name("read_row_hits_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row hits for read requests per channel per core")
            .precision(0)
            ;
        read_row_misses
            .init(configs.get_int("cores"))
            .name("read_row_misses_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row misses for read requests per channel per core")
            .precision(0)
            ;
        read_row_conflicts
            .init(configs.get_int("cores"))
            .name("read_row_conflicts_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row conflicts for read requests per channel per core")
            .precision(0)
            ;

        write_row_hits
            .init(configs.get_int("cores"))
            .name("write_row_hits_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row hits for write requests per channel per core")
            .precision(0)
            ;
        write_row_misses
            .init(configs.get_int("cores"))
            .name("write_row_misses_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row misses for write requests per channel per core")
            .precision(0)
            ;
        write_row_conflicts
            .init(configs.get_int("cores"))
            .name("write_row_conflicts_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row conflicts for write requests per channel per core")
            .precision(0)
            ;

        useless_activates
            .name("useless_activates_"+to_string(channel->id)+ "_core")
            .desc("Number of useless activations. E.g, ACT -> PRE w/o RD or WR")
            .precision(0)
            ;

        read_transaction_bytes
            .name("read_transaction_bytes_"+to_string(channel->id))
            .desc("The total byte of read transaction per channel")
            .precision(0)
            ;
        write_transaction_bytes
            .name("write_transaction_bytes_"+to_string(channel->id))
            .desc("The total byte of write transaction per channel")
            .precision(0)
            ;

        read_latency_sum
            .name("read_latency_sum_"+to_string(channel->id))
            .desc("The memory latency cycles (in memory time domain) sum for all read requests in this channel")
            .precision(0)
            ;
        read_latency_sum_per_core
            .init(configs.get_int("cores"))
            .name("read_latency_sum_per_core"+to_string(channel->id))
            .desc("The memory latency cycles (in memory time domain) sum for all read requests per core")
            .precision(0)
            ;
        read_latency_avg
            .name("read_latency_avg_"+to_string(channel->id))
            .desc("The average memory latency cycles (in memory time domain) per request for all read requests in this channel")
            .precision(6)
            ;
        read_latency_avg_per_core
            .init(configs.get_int("cores"))
            .name("read_latency_avg_per_core"+to_string(channel->id))
            .desc("The average memory latency cycles (in memory time domain) per request per each core")
            .precision(6)
            ;
        req_queue_length_sum
            .name("req_queue_length_sum_"+to_string(channel->id))
            .desc("Sum of read and write queue length per memory cycle per channel.")
            .precision(0)
            ;
        req_queue_length_avg
            .name("req_queue_length_avg_"+to_string(channel->id))
            .desc("Average of read and write queue length per memory cycle per channel.")
            .precision(6)
            ;

        read_req_queue_length_sum
            .name("read_req_queue_length_sum_"+to_string(channel->id))
            .desc("Read queue length sum per memory cycle per channel.")
            .precision(0)
            ;
        read_req_queue_length_avg
            .name("read_req_queue_length_avg_"+to_string(channel->id))
            .desc("Read queue length average per memory cycle per channel.")
            .precision(6)
            ;

        write_req_queue_length_sum
            .name("write_req_queue_length_sum_"+to_string(channel->id))
            .desc("Write queue length sum per memory cycle per channel.")
            .precision(0)
            ;
        write_req_queue_length_avg
            .name("write_req_queue_length_avg_"+to_string(channel->id))
            .desc("Write queue length average per memory cycle per channel.")
            .precision(6)
            ;

        // CROW
        crow_invPRE
            .name("crow_invPRE_channel_"+to_string(channel->id) + "_core")
            .desc("Number of Precharge commands issued to be able to activate an entry in the CROW table.")
            .precision(0)
            ;

        crow_invACT
            .name("crow_invACT_channel_"+to_string(channel->id) + "_core")
            .desc("Number of Activate command issued to fully activate the entry to invalidate from the CROW table.")
            .precision(0)
            ;

        crow_full_restore
            .name("crow_full_restore_channel_"+to_string(channel->id) + "_core")
            .desc("Number of Activate commands issued to fully restore an entry that is being discarded due to inserting a new entry.")
            .precision(0)
            ;

        crow_skip_full_restore
            .name("crow_skip_full_restore_channel_"+to_string(channel->id) + "_core")
            .desc("Number of times full restore was not needed (FR bit not set) when discarding an entry due to inserting a new one.")
            .precision(0)
            ;

        crow_num_hits
            .name("crow_num_hits_channel_"+to_string(channel->id) + "_core")
            .desc("Number of hits to the CROW table (without additional activations needed for full restoration).")
            .precision(0)
            ;

        crow_num_all_hits
            .name("crow_num_all_hits_channel_"+to_string(channel->id) + "_core")
            .desc("Number of hits to the CROW table.")
            .precision(0)
            ;

        crow_num_misses
            .name("crow_num_misses_channel_"+to_string(channel->id) + "_core")
            .desc("Number of misses to the CROW table.")
            .precision(0)
            ;

        crow_num_copies
            .name("crow_num_copies_channel_"+to_string(channel->id) + "_core")
            .desc("Number of row copy operation CROW performed.")
            .precision(0)
            ;

        crow_num_fr_set
            .name("crow_num_fr_set_channel_"+to_string(channel->id) + "_core")
            .desc("Number of times FR bit is set when precharging.")
            .precision(0)
            ;
        crow_num_fr_notset
            .name("crow_num_fr_notset_channel_"+to_string(channel->id) + "_core")
            .desc("Number of times FR bit is not set when precharging.")
            .precision(0)
            ;
        crow_num_fr_ref
            .name("crow_num_fr_ref_channel_"+to_string(channel->id) + "_core")
            .desc("Number of times FR bit is set since the row won't be refreshed soon.")
            .precision(0)
            ;
        crow_num_fr_restore
            .name("crow_num_fr_restore_channel_"+to_string(channel->id) + "_core")
            .desc("Number of times FR bit is set since the row is not fully restored.")
            .precision(0)
            ;

        crow_num_hits_with_fr
            .name("crow_num_hits_with_fr_channel_"+to_string(channel->id) + "_core")
            .desc("Number of CROWTable hits to FR bit set entries.")
            .precision(0)
            ;

        crow_idle_cycle_due_trcd
            .name("crow_cycles_trcd_stall_channel_"+to_string(channel->id) + "_core")
            .desc("Number of cycles for which the command bus was idle but there was a request waiting for tRCD.")
            .precision(0)
            ;

        crow_idle_cycle_due_tras
            .name("crow_cycles_tras_stall_channel_"+to_string(channel->id) + "_core")
            .desc("Number of cycles for which the command bus was idle but there was a request waiting for tRAS.")
            .precision(0)
            ;

        crow_bypass_copying
            .name("crow_bypass_copying_channel_"+to_string(channel->id) + "_core")
            .desc("Number of rows not copied to a copy row due to having only rows above the hit threshold already cached.")
            .precision(0)
            ;

        tl_dram_invalidate_due_to_write
            .name("tl_dram_invalidate_due_to_write_channel_"+to_string(channel->id) + "_core")
            .desc("Number of TL-DRAM cached rows invalidated during activation due to pending writes.")
            .precision(0)
            ;

        tl_dram_precharge_cached_row_due_to_write
            .name("tl_dram_precharge_cached_row_due_to_write_channel_"+to_string(channel->id) + "_core")
            .desc("Number of TL-DRAM cached rows precharged to write data.")
            .precision(0)
            ;

        tl_dram_precharge_failed_due_to_timing
            .name("tl_dram_precharge_failed_due_to_timing_channel_"+to_string(channel->id) + "_core")
            .desc("Number of cycles failed to issue a PRE command to TL-DRAM cache row.")
            .precision(0)
            ;
        // END - CROW

#ifndef INTEGRATED_WITH_GEM5
        record_read_hits
            .init(configs.get_int("cores"))
            .name("record_read_hits")
            .desc("record read hit count for this core when it reaches request limit or to the end")
            ;

        record_read_misses
            .init(configs.get_int("cores"))
            .name("record_read_misses")
            .desc("record_read_miss count for this core when it reaches request limit or to the end")
            ;

        record_read_conflicts
            .init(configs.get_int("cores"))
            .name("record_read_conflicts")
            .desc("record read conflict count for this core when it reaches request limit or to the end")
            ;

        record_write_hits
            .init(configs.get_int("cores"))
            .name("record_write_hits")
            .desc("record write hit count for this core when it reaches request limit or to the end")
            ;

        record_write_misses
            .init(configs.get_int("cores"))
            .name("record_write_misses")
            .desc("record write miss count for this core when it reaches request limit or to the end")
            ;

        record_write_conflicts
            .init(configs.get_int("cores"))
            .name("record_write_conflicts")
            .desc("record write conflict for this core when it reaches request limit or to the end")
            ;
        record_read_latency_avg_per_core
            .init(configs.get_int("cores"))
            .name("record_read_latency_avg_"+to_string(channel->id))
            .desc("The memory latency cycles (in memory time domain) average per core in this channel")
            .precision(6)
            ;

#endif
    }

    ~Controller(){
        delete scheduler;
        delete rowpolicy;
        delete rowtable;
        delete channel;
        delete refresh;
        for (auto& file : cmd_trace_files)
            file.close();
        cmd_trace_files.clear();

        delete crow_table;
        delete[] ref_counters;
    }

    void finish(long read_req, long dram_cycles) {
      read_latency_avg = read_latency_sum.value() / read_req;
      req_queue_length_avg = req_queue_length_sum.value() / dram_cycles;
      read_req_queue_length_avg = read_req_queue_length_sum.value() / dram_cycles;
      write_req_queue_length_avg = write_req_queue_length_sum.value() / dram_cycles;

	  for(int coreid = 0; coreid < read_latency_avg_per_core.size() ; coreid++){
	  	read_latency_avg_per_core[coreid] = read_latency_sum_per_core[coreid].value()/
                                        (float)(read_row_hits[coreid].value() + read_row_misses[coreid].value() +
                                                read_row_conflicts[coreid].value());
	  }

      // call finish function of each channel
      channel->finish(dram_cycles);

      // print out the row_act_hist
      if(collect_row_act_histogram) {
          printf("Printing Row Activation Histogram\n");
          printf("Format: row_id, access_count\n");
          printf("=================================\n");
        for(int bank_id = 0; bank_id < 8; bank_id++) {
            for(int sa_id = 0; sa_id < num_SAs; sa_id++) {
                printf("----- Bank %d, Subarray %d\n", bank_id, sa_id);
                auto& cur_hist = row_act_hist[bank_id][sa_id];

                for(auto it = cur_hist.begin(); it != cur_hist.end(); it++) {
                    printf("%d, %d\n", it->first, it->second);           
                }
            }

        }
      }
    }

    /* Member Functions */
    Queue& get_queue(Request::Type type)
    {
        switch (int(type)) {
            case int(Request::Type::READ):
            case int(Request::Type::PREFETCH): return readq;
            case int(Request::Type::WRITE): return writeq;
            default: return otherq;
        }
    }

    bool enqueue(Request& req)
    {
        Queue& queue = get_queue(req.type);
        if (queue.max == queue.size())
            return false;

        req.arrive = clk;
        queue.q.push_back(req);
        // shortcut for read requests, if a write to same addr exists
        // necessary for coherence
        if ((req.type == Request::Type::READ || req.type == Request::Type::PREFETCH) && find_if(writeq.q.begin(), writeq.q.end(),
                [req](Request& wreq){ return req.addr == wreq.addr;}) != writeq.q.end()){
            req.depart = clk + 1;
            pending.push_back(req);
            readq.q.pop_back();
        }

        return true;
    }

    bool upgrade_prefetch_req(const Request& req) {
        assert(req.type == Request::Type::READ);

        Queue& queue = get_queue(req.type);

        // the prefetch request could be in readq, actq, or pending
        if (upgrade_prefetch_req(queue, req))
            return true;

        if (upgrade_prefetch_req(actq, req))
            return true;

        if (upgrade_prefetch_req(pending, req))
            return true;

        return false;
    } 

    void tick()
    {
        clk++;
        req_queue_length_sum += readq.size() + writeq.size() + pending.size();
        read_req_queue_length_sum += readq.size() + pending.size();
        write_req_queue_length_sum += writeq.size();

        /*** 1. Serve completed reads ***/
        if (pending.size()) {
            Request& req = pending[0];
            if (req.depart <= clk) {
                if (req.depart - req.arrive > 1) { // this request really accessed a row
                  read_latency_sum += req.depart - req.arrive;
                  read_latency_sum_per_core[req.coreid] += (req.depart - req.arrive);
                  channel->update_serving_requests(
                      req.addr_vec.data(), -1, clk);
                }
                
                req.callback(req);
                pending.pop_front();
            }
        }

        /*** 2. Refresh scheduler ***/
        if (!refresh_disabled)
            refresh->tick_ref();

        /*** 3. Should we schedule writes? ***/
        if (!write_mode) {
            // yes -- write queue is almost full or read queue is empty
            if (writeq.size() >= int(0.8 * writeq.max) /*|| readq.size() == 0*/){
                write_mode = true;
            }
        }
        else {
            // no -- write queue is almost empty and read queue is not empty
            if (writeq.size() <= int(0.2 * writeq.max) && readq.size() != 0) {
                write_mode = false;
            }
        }

        /*** 4. Find the best command to schedule, if any ***/

        // First check the actq (which has higher priority) to see if there
        // are requests available to service in this cycle
        Queue* queue = &actq;
        typename T::Command cmd;

        auto req = scheduler->get_head(queue->q);

        bool is_valid_req = (req != queue->q.end());

        if(is_valid_req) {
            cmd = get_first_cmd(req);
            is_valid_req = is_ready(cmd, req->addr_vec);
        }


        if (!is_valid_req) {

            queue = !write_mode ? &readq : &writeq;

            if (otherq.size())
                queue = &otherq;  // "other" requests are rare, so we give them precedence over reads/writes

            req = scheduler->get_head(queue->q);

            is_valid_req = (req != queue->q.end());

            if(is_valid_req){

                cmd = get_first_cmd(req);
                is_valid_req = is_ready(cmd, req->addr_vec);
            }
        }
        
        if (!is_valid_req) {
            // we couldn't find a command to schedule -- let's try to be speculative
            auto cmd = T::Command::PRE;
            vector<int> victim = rowpolicy->get_victim(cmd);
            if (!victim.empty())
                issue_cmd(cmd, victim);

            return;  // nothing more to be done this cycle
        }

        if (req->is_first_command) {
            req->is_first_command = false;
            int coreid = req->coreid;
            if (req->type == Request::Type::READ || req->type == Request::Type::WRITE || req->type == Request::Type::PREFETCH) {
              channel->update_serving_requests(req->addr_vec.data(), 1, clk);
            }
            int tx = (channel->spec->prefetch_size * channel->spec->channel_width / 8);
            if (req->type == Request::Type::READ || req->type == Request::Type::PREFETCH) {
                if (is_row_hit(req)) {
                    ++read_row_hits[coreid];
                    ++row_hits;
                } else if (is_row_open(req)) {
                    ++read_row_conflicts[coreid];
                    ++row_conflicts;
                } else {
                    ++read_row_misses[coreid];
                    ++row_misses;
                }
              read_transaction_bytes += tx;
            } else if (req->type == Request::Type::WRITE) {
              if (is_row_hit(req)) {
                  ++write_row_hits[coreid];
                  ++row_hits;
              } else if (is_row_open(req)) {
                  ++write_row_conflicts[coreid];
                  ++row_conflicts;
              } else {
                  ++write_row_misses[coreid];
                  ++row_misses;
              }
              write_transaction_bytes += tx;
            }
        }


        // CROW
        // if going to activate a new row which will discard an entry from
        // the CROW table, we may need to fully restore the discarding row
        // first

        bool make_crow_copy = true;
        if (enable_crow && channel->spec->is_opening(cmd)) {
            vector<int> target_addr_vec = get_addr_vec(cmd, req);
            if(!crow_table->is_hit(target_addr_vec) && crow_table->is_full(target_addr_vec)) {
                bool discard_next = true;

                if(prioritize_evict_fully_restored) {
                    assert(false && "Error: Unimplemented functionality!");
                }

                if(discard_next) {
                    CROWEntry* cur_entry = crow_table->get_LRU_entry(target_addr_vec, crow_evict_threshold);
                    
                    if(cur_entry == nullptr) {
                        assert(!enable_tl_dram && "Error: It should always be possible to discard an entry with TL-DRAM.");
                        make_crow_copy = false;
                        crow_bypass_copying++;
                    }
                    else {
                        if(cur_entry->FR && !enable_tl_dram) {
                            // We first need to activate the discarding row to fully
                            // restore it
                            
                            target_addr_vec[int(T::Level::Row)] = cur_entry->row_addr;


                            issue_cmd(cmd, target_addr_vec, true); 
                            crow_full_restore++;
                            return;
                        } else {
                            // move to LRU
                            target_addr_vec[int(T::Level::Row)] = cur_entry->row_addr;
                            crow_table->make_LRU(target_addr_vec, cur_entry);
                            crow_skip_full_restore++;
                        }
                    }
                }
            }
            else if (crow_table->is_hit(target_addr_vec) && enable_tl_dram) {
                if(req->type == Request::Type::WRITE){
                    crow_table->invalidate(target_addr_vec);
                    tl_dram_invalidate_due_to_write++;
                }
            }

        }

        if((enable_crow && enable_tl_dram) && ((cmd == T::Command::WR) || (cmd == T::Command::WRA))) {
            vector<int> target_addr_vec = get_addr_vec(cmd, req);
            target_addr_vec[int(T::Level::Row)] = rowtable->get_open_row(target_addr_vec);

            CROWEntry* cur_entry = crow_table->get_hit_entry(target_addr_vec);
            if(cur_entry != nullptr && cur_entry->total_hits != 0) {
                // convert the command to precharge
                cmd = T::Command::PRE;
                if(is_ready(T::Command::PRE, target_addr_vec)){
                    issue_cmd(cmd, target_addr_vec, true);
                    tl_dram_precharge_cached_row_due_to_write++;
                } else {
                    tl_dram_precharge_failed_due_to_timing++;
                }
                return;
            }
        }
        // END - CROW

        issue_cmd(cmd, get_addr_vec(cmd, req), false, make_crow_copy);

        // check whether this is the last command (which finishes the request)
        if (cmd != channel->spec->translate[int(req->type)]){
            if(channel->spec->is_opening(cmd)) {
                // promote the request that caused issuing activation to actq
                actq.q.push_back(*req);
                queue->q.erase(req);
            }

            return;
        }

        // set a future completion time for read requests
        if (req->type == Request::Type::READ || req->type == Request::Type::PREFETCH) {
            req->depart = clk + channel->spec->read_latency;
            pending.push_back(*req);
        }

        if (req->type == Request::Type::WRITE) {
            channel->update_serving_requests(req->addr_vec.data(), -1, clk);
        }

        // remove request from queue
        queue->q.erase(req);
    }

    // CROW
    bool req_hold_due_trcd(list<Request>& q) {
        // go over all requests in 'q' and check if there is one that could
        // have been issued if tRCD was 0.
        
        for (auto it = q.begin(); it != q.end(); it++) {
            if(is_ready_no_trcd(it))
                return true;
        }

        return false;
    }

    bool req_hold_due_tras(list<Request>& q) {
        // go over all requests in 'q' and check if there is one that could
        // have been issued if tRAS was 0.
        
        for (auto it = q.begin(); it != q.end(); it++) {
            if(is_ready_no_tras(it))
                return true;
        }

        return false;
    }

    bool is_ready_no_trcd(list<Request>::iterator req) {
        typename T::Command cmd = get_first_cmd(req);

        if(!channel->spec->is_accessing(cmd))
            return false;

        return channel->check_no_trcd(cmd, req->addr_vec.data(), clk);
    }

    bool is_ready_no_tras(list<Request>::iterator req) {
        typename T::Command cmd = get_first_cmd(req);

        if(cmd != T::Command::PRE)
            return false;

        return channel->check_no_tras(cmd, req->addr_vec.data(), clk);
    }
    // END - CROW

    bool is_ready(list<Request>::iterator req)
    {
        typename T::Command cmd = get_first_cmd(req);
        return channel->check_iteratively(cmd, req->addr_vec.data(), clk);
    }

    bool is_ready(typename T::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_iteratively(cmd, addr_vec.data(), clk);
    }

    bool is_row_hit(list<Request>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        typename T::Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_hit(cmd, req->addr_vec.data());
    }

    bool is_row_hit(typename T::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_row_hit(cmd, addr_vec.data());
    }

    bool is_row_open(list<Request>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        typename T::Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_open(cmd, req->addr_vec.data());
    }

    bool is_row_open(typename T::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_row_open(cmd, addr_vec.data());
    }

    void update_temp(ALDRAM::Temp current_temperature)
    {
    }

    // For telling whether this channel is busying in processing read or write
    bool is_active() {
      return (channel->cur_serving_requests > 0);
    }

    // For telling whether this channel is under refresh
    bool is_refresh() {
      return clk <= channel->end_of_refreshing;
    }

    void record_core(int coreid) {
#ifndef INTEGRATED_WITH_GEM5
      record_read_hits[coreid] = read_row_hits[coreid];
      record_read_misses[coreid] = read_row_misses[coreid];
      record_read_conflicts[coreid] = read_row_conflicts[coreid];
      record_write_hits[coreid] = write_row_hits[coreid];
      record_write_misses[coreid] = write_row_misses[coreid];
      record_write_conflicts[coreid] = write_row_conflicts[coreid];
      record_read_latency_avg_per_core[coreid] = read_latency_sum_per_core[coreid].value()/
                                        (float)(read_row_hits[coreid].value() + read_row_misses[coreid].value() + 
                                                read_row_conflicts[coreid].value());
#endif
    }

    void reload_options(const Config& configs) {

        if((channel->spec->standard_name == "SALP-MASA") || (channel->spec->standard_name == "SALP-1") || 
                (channel->spec->standard_name == "SALP-2"))
            channel->update_num_subarrays(configs.get_int("subarrays"));

        prioritize_evict_fully_restored = configs.get_bool("crow_evict_fully_restored");
        collect_row_act_histogram = configs.get_bool("collect_row_activation_histogram");
        copy_rows_per_SA = configs.get_int("copy_rows_per_SA");
        weak_rows_per_SA = configs.get_int("weak_rows_per_SA");
        refresh_mult = configs.get_float("refresh_mult");
        crow_evict_threshold = configs.get_int("crow_entry_evict_hit_threshold");
        crow_half_life = configs.get_int("crow_half_life");
        crow_to_mru_frac = configs.get_float("crow_to_mru_frac");
        crow_table_grouped_SAs = configs.get_int("crow_table_grouped_SAs");
        
        refresh_disabled = configs.get_bool("disable_refresh"); 

        if (copy_rows_per_SA > 0)
            enable_crow = true;

        if (configs.get_bool("enable_crow_upperbound")) {
            enable_crow_upperbound = true;
            enable_crow = false;
        }

        enable_tl_dram = configs.get_bool("enable_tl_dram");

        if(enable_crow || enable_crow_upperbound) {

            // Adjust tREFI
            channel->spec->speed_entry.nREFI *= refresh_mult;
            
            trcd_crow_partial_hit = configs.get_float("trcd_crow_partial_hit");
            trcd_crow_full_hit = configs.get_float("trcd_crow_full_hit");

            tras_crow_partial_hit_partial_restore = configs.get_float("tras_crow_partial_hit_partial_restore");
            tras_crow_partial_hit_full_restore = configs.get_float("tras_crow_partial_hit_full_restore");
            tras_crow_full_hit_partial_restore = configs.get_float("tras_crow_full_hit_partial_restore");
            tras_crow_full_hit_full_restore = configs.get_float("tras_crow_full_hit_full_restore");
            tras_crow_copy_partial_restore = configs.get_float("tras_crow_copy_partial_restore");
            tras_crow_copy_full_restore = configs.get_float("tras_crow_copy_full_restore");

            twr_partial_restore = configs.get_float("twr_partial_restore");
            twr_full_restore = configs.get_float("twr_full_restore");

            initialize_crow();

        }

        if(collect_row_act_histogram)
            assert(num_SAs <= 128);
    }


private:
    typename T::Command get_first_cmd(list<Request>::iterator req)
    {
        typename T::Command cmd = channel->spec->translate[int(req->type)];
        return channel->decode_iteratively(cmd, req->addr_vec.data());
    }

    unsigned long last_clk = 0; // DEBUG
    unsigned long num_cas_cmds = 0;
    void issue_cmd(typename T::Command cmd, const vector<int>& addr_vec, bool do_full_restore = false, bool make_crow_copy = true)
    {
        assert(is_ready(cmd, addr_vec));

        if(warmup_complete && collect_row_act_histogram) {
            if(channel->spec->is_opening(cmd)) {
                int row_id = addr_vec[int(T::Level::Row)];
                int SA_size = channel->spec->org_entry.count[int(T::Level::Row)]/num_SAs;
                int sa_id = row_id/SA_size;
                
                auto& cur_hist = row_act_hist[addr_vec[int(T::Level::Bank)]][sa_id];

                int local_row_id = row_id % SA_size;
                if(cur_hist.find(local_row_id) == cur_hist.end())
                    cur_hist[local_row_id] = 1;
                else
                    cur_hist[local_row_id]++;
            }
        }



        // CROW
        int crow_hit = 0; // 0 -> no hit, 1 -> hit to partially restored row, 2 -> hit to fully restored row
        bool crow_copy = false;

        if(enable_crow || enable_crow_upperbound) {
            if(channel->spec->is_opening(cmd)) {
                if(enable_crow_upperbound || crow_table->is_hit(addr_vec)) {
                    assert(make_crow_copy && "Error: A row activation without copying should not hit on CROWTable!");

                    bool require_full_restore = false;
                   
                    if(!enable_crow_upperbound)
                        require_full_restore = crow_table->get_hit_entry(addr_vec)->FR;

                    if(require_full_restore)
                        crow_hit = 1;
                    else
                        crow_hit = 2;


                    if(!do_full_restore) {
                        crow_num_hits++;
                        if(enable_crow)
                            crow_table->access(addr_vec); // we shouldn't access it if we do additional activation to fully restore
                    } else {
                        crow_table->access(addr_vec, true); // move the fully restored row to LRU position
                    }
                    
                    crow_num_all_hits++;

                    if(!enable_crow_upperbound && require_full_restore){
                        crow_num_hits_with_fr++;
                    }
                } else {
                    assert(!do_full_restore && "Full restoration should be issued only when the row is in the CROW table.");
                    assert((!enable_tl_dram || make_crow_copy) && "Error: ACT command should always copy when TL-DRAM is used.");
                    // crow miss
                    if(make_crow_copy) {
                        crow_copy = true;
                        crow_table->add_entry(addr_vec, false);
                        crow_num_copies++;
                    } else {
                        crow_table->access(addr_vec); // we know it is a miss but we access to update the hit_count of the LRU entry
                    }
                    
                    crow_num_misses++;
                }
            }

            if((cmd == T::Command::WR) || (cmd == T::Command::WRA)) {
               if(enable_crow_upperbound)
                  crow_hit = 2;
               else if(crow_table->is_hit(addr_vec)) {
                   if(crow_table->get_hit_entry(addr_vec)->FR)
                       crow_hit = 1;
                   else
                       crow_hit = 2;
               }
               else
                   // assert(false && "Can this happen?"); // Hasan:
                   // happens right after warmup when a row was opened
                   // before CROWTable initialization. Printing the message
                   // below to log how many times this happens to make sure
                   // it does not happen many times due to a different
                   // reason
                   //printf("Warning: Ramulator: Writing on a CROWTable miss!\n");
                   crow_hit = -1;
            }

            if((!enable_crow_upperbound && !enable_tl_dram) && channel->spec->is_closing(cmd)) {
                crow_set_FR_on_PRE(cmd, addr_vec);
            }

            
            if(!enable_crow_upperbound && channel->spec->is_refreshing(cmd)) {
                                
                int nREFI = channel->spec->speed_entry.nREFI;
                float tCK = channel->spec->speed_entry.tCK;
                int base_refw = is_LPDDR4 ? 32*refresh_mult : 64*refresh_mult;
                ulong ticks_in_ref_int = base_refw*1000000/tCK;
                int num_refs = ticks_in_ref_int/nREFI;

               
                int num_rows_refreshed_at_once;
                int num_ref_limit;
               
               if(is_DDR4 || is_LPDDR4) {
                  num_ref_limit = channel->spec->org_entry.count[int(T::Level::Row)];
                  num_rows_refreshed_at_once = ceil(channel->spec->org_entry
                                                    .count[int(T::Level::Row)]/(float)num_refs);
               }
               else { // For SALP
                   num_ref_limit = channel->spec->org_entry.count[int(T::Level::Row)] * 
                                    channel->spec->org_entry.count[int(T::Level::Row) - 1];
                   num_rows_refreshed_at_once = ceil(channel->spec->org_entry
                                                    .count[int(T::Level::Row)] * channel->spec->org_entry.count[int(T::Level::Row) - 1]/(float)num_refs);
               }

                ref_counters[addr_vec[int(T::Level::Rank)]] += num_rows_refreshed_at_once;

                if(ref_counters[addr_vec[int(T::Level::Rank)]] >= num_ref_limit)
                    ref_counters[addr_vec[int(T::Level::Rank)]] = 0;
            }

            switch(crow_hit) {
                case 0:
                    if(crow_copy){
                        assert(make_crow_copy && "Error: Using copy timing parameters for regular access!");
                        load_timing(channel, crow_copy_timing);
                    }
                    break;
                case 1: // partial hit
                    assert(!crow_copy && "Error: A row should not be copied if is already duplicated!");

                    if(do_full_restore){
                        assert(cmd == T::Command::ACT);
                        assert(!enable_crow_upperbound);
                        load_timing(channel, partial_crow_hit_full_restore_timing); 
                    }
                    else
                        load_timing(channel, partial_crow_hit_partial_restore_timing);
                    break;
                case 2: // hit to a fully restored row
                    assert(!crow_copy && "Error: A row should not be copied if is already duplicated!");

                    if(do_full_restore){
                        assert(cmd == T::Command::ACT);
                        assert(!enable_crow_upperbound);
                        load_timing(channel, full_crow_hit_full_restore_timing);
                    }
                    else
                        load_timing(channel, full_crow_hit_partial_restore_timing);
            }
            
        }

        // END - CROW
        
        channel->update(cmd, addr_vec.data(), clk);

        if(enable_crow && do_full_restore && (cmd == T::Command::ACT)) {
            // clean just_opened state
            if(is_LPDDR4) {
                channel->children[addr_vec[int(T::Level::Rank)]]->
                    children[addr_vec[int(T::Level::Bank)]]->just_opened = false;
            }
            else {
                assert(false && "Not implemented for the current DRAM standard.");
            }
        }

        // CROW
        load_default_timing(channel);
        // END - CROW


        if(cmd == T::Command::PRE){
            if(rowtable->get_hits(addr_vec, true) == 0){
                useless_activates++;
            }
        }
        
        rowtable->update(cmd, addr_vec, clk);
        
        if (record_cmd_trace){
            // select rank
            auto& file = cmd_trace_files[addr_vec[1]];
            string& cmd_name = channel->spec->command_name[int(cmd)];

            if(cmd_name != "SASEL") {
                if((cmd_name == "ACT") && (crow_copy || (crow_hit > 0)) && (!enable_crow_upperbound))
                    file << clk << ',' << "ACTD";
                else
                    file << clk << ',' << cmd_name;
                // TODO bad coding here
                if (cmd_name == "PREA" || cmd_name == "REF")
                    file<<endl;
                else{
                    int bank_id = 0;
                    if(channel->spec->standard_name == "SALP-MASA")
                        bank_id = addr_vec[int(T::Level::Bank)]*channel->spec->org_entry.count[int(T::Level::Bank) + 1] + addr_vec[int(T::Level::Bank) + 1];
                    else
                        bank_id = addr_vec[int(T::Level::Bank)];
                    if (channel->spec->standard_name == "DDR4" || channel->spec->standard_name == "GDDR5")
                        bank_id += addr_vec[int(T::Level::Bank) - 1] * channel->spec->org_entry.count[int(T::Level::Bank)];
                    file<<','<<bank_id<<endl;
                }
            }
        }
        if (print_cmd_trace){
            printf("%5s %10ld:", channel->spec->command_name[int(cmd)].c_str(), clk);
            for (int lev = 0; lev < int(T::Level::MAX); lev++)
                printf(" %5d", addr_vec[lev]);
            printf("\n");
        }
    }
    vector<int> get_addr_vec(typename T::Command cmd, list<Request>::iterator req){
        return req->addr_vec;
    }

    void load_timing(DRAM<T>* node, vector<typename T::TimingEntry> timing[][int(T::Command::MAX)]) {
        node->set_timing(timing[int(node->level)]);

        for(auto child : node->children)
            load_timing(child, timing);
    }

    void load_default_timing(DRAM<T>* node) {
        node->set_timing(node->spec->timing[int(node->level)]);

        for(auto child : node->children)
            load_default_timing(child);
    }

    void initialize_crow() {

        // 1. timing for CROW table hit on partially restored row when
        // intended to partially restore
        initialize_crow_timing(partial_crow_hit_partial_restore_timing, trcd_crow_partial_hit, 
                tras_crow_partial_hit_partial_restore, twr_partial_restore, 1.0f /*crow_hit_tfaw*/);

        // 2. timing for CROW table hit on partially restored row when
        // intended to fully restore
        initialize_crow_timing(partial_crow_hit_full_restore_timing, trcd_crow_partial_hit, 
                tras_crow_partial_hit_full_restore, twr_full_restore, 1.0f);

        // 3. timing for CROW table hit on fully restored row when
        // intended to partially restore
        initialize_crow_timing(full_crow_hit_partial_restore_timing, trcd_crow_full_hit, 
                tras_crow_full_hit_partial_restore, twr_partial_restore, 1.0f);

        // 4. timing for CROW table hit on fully restored row when
        // intended to fully restore
        initialize_crow_timing(full_crow_hit_full_restore_timing, trcd_crow_full_hit, 
                tras_crow_full_hit_full_restore, twr_full_restore, 1.0f);

        // 5. timing for CROW copy
        initialize_crow_timing(crow_copy_timing, 1.0f, tras_crow_copy_full_restore, twr_full_restore, 1.0f);
                
        if(enable_crow_upperbound)
            return;

        if(crow_table != nullptr)
            delete crow_table;

        crow_table = new CROWTable<T>(channel->spec, channel->id, num_SAs, copy_rows_per_SA, 
                weak_rows_per_SA, crow_evict_threshold, crow_half_life, crow_to_mru_frac,
                crow_table_grouped_SAs);

        ref_counters = new int[channel->spec->org_entry.count[int(T::Level::Rank)]];
        for(int i = 0; i < channel->spec->org_entry.count[int(T::Level::Rank)]; i++)
            ref_counters[i] = 0;
    }

    void initialize_crow_timing(vector<typename T::TimingEntry> timing[][int(T::Command::MAX)], const float trcd_factor, 
                    const float tras_factor, const float twr_factor, const float tfaw_factor) {

        // if(timing == partial_crow_hit_partial_restore_timing)
        //     printf("Initializing Partial CROW Hit to Partial Restoration Timing... \n");
        // else if(timing == partial_crow_hit_full_restore_timing)
        //     printf("Initializing Partial CROW Hit to Full Restoration Timing... \n");
        // else if(timing == full_crow_hit_partial_restore_timing)
        //     printf("Initializing Full CROW Hit to Partial Restoration Timing... \n");
        // else if(timing == full_crow_hit_full_restore_timing)
        //     printf("Initializing Full CROW Hit to Full Restoration Timing... \n");
        // else if(timing == crow_copy_timing)
        //     printf("Initializing CROW Copy Timing... \n");
        // else
        //     assert(false && "Initializing unknown CROW timing.");


        // copy the default timing parameters
        for(uint l = 0; l < int(T::Level::MAX); l++) {
            for(uint c = 0; c < int(T::Command::MAX); c++) {
                timing[l][c] = channel->spec->timing[l][c];
            }
        }

        vector<typename T::TimingEntry>* t;
        int trcd = 0, tras = 0;

        // apply trcd_factor to the related timing params
        t = timing[int(T::Level::Bank)];

        for (auto& t : t[int(T::Command::ACT)]) {
            if((t.cmd == T::Command::RD) || (t.cmd == T::Command::RDA)){
                //printf("Default ACT-to-RD cycles: %d\n", t.val);
                t.val = (int)ceil(t.val * trcd_factor);
                trcd = t.val;
                //printf("New ACT-to-RD cycles: %d\n", t.val);
            }

            if((t.cmd == T::Command::WR) || (t.cmd == T::Command::WRA)) {
                //printf("Default ACT-to-WR cycles: %d\n", t.val);
                t.val = (int)ceil(t.val * trcd_factor);
                //printf("New ACT-to-WR cycles: %d\n", t.val);
            }
        }

        // apply tras_factor to the related timing parameters
        t = timing[int(T::Level::Rank)];

        for (auto& t : t[int(T::Command::ACT)]) {
            if(t.cmd == T::Command::PREA){
               //printf("Default ACT-to-PREA cycles: %d\n", t.val);
               t.val = (int)ceil(t.val * tras_factor);
               tras = t.val;
               //printf("New ACT-to-PREA cycles: %d\n", t.val);
            }
        }

        t = timing[int(T::Level::Bank)];

        for (auto& t : t[int(T::Command::ACT)]) {
            if(t.cmd == T::Command::PRE) {
                //printf("Default ACT-to-PRE cycles: %d\n", t.val);
                t.val = (int)ceil(t.val * tras_factor);
                //printf("New ACT-to-PRE cycles: %d\n", t.val);
            }
        }

        // apply both trcd_factor and tras_factor to tRC
        assert(trcd != 0 && tras !=0 && "tRCD or tRAS was not set.");
        t = timing[int(T::Level::Bank)];

        for (auto& t : t[int(T::Command::ACT)]) {
            if(t.cmd == T::Command::ACT) {
                //printf("Default ACT-to-ACT cycles: %d\n", t.val);
                t.val = trcd + tras; 
                //printf("New ACT-to-ACT cycles: %d\n", t.val);
            }
        }

        // apply twr_factor to the related timing parameters
        t = timing[int(T::Level::Rank)];

        for (auto& t : t[int(T::Command::WR)]) {
            if(t.cmd == T::Command::PREA) {
                //printf("Default WR-to-PREA cycles: %d\n", t.val);
                t.val = channel->spec->speed_entry.nCWL + channel->spec->speed_entry.nBL +
                            (int)ceil(channel->spec->speed_entry.nWR*twr_factor);
                //printf("New WR-to-PREA cycles: %d\n", t.val);
            }
        }


        t = timing[int(T::Level::Bank)];

        for (auto& t : t[int(T::Command::WR)]) {
            if(t.cmd == T::Command::PRE) {
                //printf("Default WR-to-PRE cycles: %d\n", t.val);
                t.val = channel->spec->speed_entry.nCWL + channel->spec->speed_entry.nBL +
                            (int)ceil(channel->spec->speed_entry.nWR*twr_factor);
                //printf("New WR-to-PRE cycles: %d\n", t.val);
            }
        }

        for (auto& t : t[int(T::Command::WRA)]) {
            if(t.cmd == T::Command::ACT) {
                //printf("Default WRA-to-ACT cycles: %d\n", t.val);
                t.val = channel->spec->speed_entry.nCWL + channel->spec->speed_entry.nBL +
                            (int)ceil(channel->spec->speed_entry.nWR*twr_factor) +
                            channel->spec->speed_entry.nRP;
                //printf("New WRA-to-ACT cycles: %d\n", t.val);
            }

        }

        // apply tfaw_factor to the related timing parameters
        t = timing[int(T::Level::Rank)];

        for (auto& t : t[int(T::Command::ACT)]) {
            if(t.cmd == T::Command::ACT && (t.dist == 4)) {
                //printf("Default ACT-to-ACT (tFAW) cycles: %d\n", t.val);
                t.val = (int)ceil(t.val*tfaw_factor);
                //printf("New ACT-to-ACT (tFAW) cycles: %d\n", t.val);
            }

        }
    }

    void crow_set_FR_on_PRE(typename T::Command cmd, const vector<int>& addr_vec) {
        
        if(cmd != T::Command::PRE) {
            vector<int> cur_addr_vec = addr_vec;

            int bank_levels = int(T::Level::Bank) - int(T::Level::Rank);

            switch(bank_levels) {
                case 1:
                    for(int i = 0; i < channel->spec->org_entry.count[int(T::Level::Bank)]; i++) {
                        cur_addr_vec[int(T::Level::Bank)] = i;
                        crow_set_FR_on_PRE_single_bank(cur_addr_vec);
                    }
                    break;
                case 2:
                    for(int i = 0; i < channel->spec->org_entry.count[int(T::Level::Bank) - 1]; 
                            i++) {
                        cur_addr_vec[int(T::Level::Bank) - 1] = i;
                        for(int j = 0; j < channel->spec->org_entry.count[int(T::Level::Bank)]; 
                                j++) {
                            cur_addr_vec[int(T::Level::Bank)] = j;
                            crow_set_FR_on_PRE_single_bank(cur_addr_vec);
                        }
                    }

                    break;
                default:
                    assert(false && "Not implemented!");
            }
        } else {
            crow_set_FR_on_PRE_single_bank(addr_vec);
        }

    }

    void crow_set_FR_on_PRE_single_bank(const vector<int>& addr_vec) {
        
        // get the id of the row to be precharged
        int pre_row = rowtable->get_open_row(addr_vec);

        if(pre_row == -1)
            return;

        auto crow_addr_vec = addr_vec;
        crow_addr_vec[int(T::Level::Row)] = pre_row;

        if(!crow_table->is_hit(crow_addr_vec)) // An active row may not be in crow_table right after the warmup period finished
            return;


        // get the next SA to be refreshed
        const float ref_period_threshold = 0.4f;
        
        int nREFI = channel->spec->speed_entry.nREFI;
        float tCK = channel->spec->speed_entry.tCK;
        int SA_size;
        int next_SA_to_ref;
        
        
        if(is_DDR4 || is_LPDDR4) {
           SA_size = channel->spec->org_entry.count[int(T::Level::Row)]/num_SAs;
        }
        else { // for SALP
            SA_size = channel->spec->org_entry.count[int(T::Level::Row)];
        }
        
        next_SA_to_ref = ref_counters[addr_vec[int(T::Level::Rank)]]/SA_size;
        
        float trefi_in_ns = nREFI * tCK;

        int SA_id;
        if(is_DDR4 || is_LPDDR4)
            SA_id = pre_row/SA_size;
        else // for SALP
            SA_id = addr_vec[int(T::Level::Row) - 1];

               
        int base_refw = is_LPDDR4 ? 32*refresh_mult : 64*refresh_mult; 
        ulong ticks_in_ref_int = (base_refw)*1000000/tCK;
        int num_refs = ticks_in_ref_int/nREFI;
        
        int rows_per_bank;

        if(is_DDR4 || is_LPDDR4)
            rows_per_bank = channel->spec->org_entry.count[int(T::Level::Row)];
        else // for SALP
            rows_per_bank = channel->spec->org_entry.count[int(T::Level::Row)] * channel->spec->org_entry.count[int(T::Level::Row) - 1];

        int num_rows_refreshed_at_once = ceil(rows_per_bank/(float)num_refs);

        int SA_diff = (next_SA_to_ref > SA_id) ? (num_SAs - (next_SA_to_ref - SA_id)) : SA_id - next_SA_to_ref;

        long time_diff = (long)(SA_diff * (trefi_in_ns*(SA_size/(float)num_rows_refreshed_at_once)));
        bool refresh_check = time_diff > ((base_refw*1000000*refresh_mult) * ref_period_threshold);


        // get the restoration time applied
        long applied_restoration = channel->cycles_since_last_act(addr_vec, clk);
        bool restore_check = (applied_restoration < channel->spec->speed_entry.nRAS);

        if(refresh_check && restore_check)
            crow_num_fr_set++;
        else
            crow_num_fr_notset++;

        if(refresh_check)
            crow_num_fr_ref++;

        if(restore_check)
            crow_num_fr_restore++;


        crow_table->set_FR(crow_addr_vec, refresh_check && restore_check);

    }

    bool upgrade_prefetch_req (Queue& q, const Request& req) {
        if(q.size() == 0)
            return false;

        auto pref_req = find_if(q.q.begin(), q.q.end(), [req](Request& preq) {
                                                return req.addr == preq.addr;});

        if (pref_req != q.q.end()) {
            pref_req->type = Request::Type::READ;
            pref_req->callback = pref_req->proc_callback; // FIXME: proc_callback is an ugly workaround
            return true;
        }
            
        return false;
    }

    // FIXME: ugly
    bool upgrade_prefetch_req (deque<Request>& p, const Request& req) {
        if (p.size() == 0)
            return false;

        auto pref_req = find_if(p.begin(), p.end(), [req](Request& preq) {
                                                return req.addr == preq.addr;});

        if (pref_req != p.end()) {
            pref_req->type = Request::Type::READ;
            pref_req->callback = pref_req->proc_callback; // FIXME: proc_callback is an ugly workaround
            return true;
        }
            
        return false;
    }

    vector<typename T::TimingEntry> partial_crow_hit_partial_restore_timing[int(T::Level::MAX)][int(T::Command::MAX)];
    vector<typename T::TimingEntry> partial_crow_hit_full_restore_timing[int(T::Level::MAX)][int(T::Command::MAX)];
    vector<typename T::TimingEntry> full_crow_hit_partial_restore_timing[int(T::Level::MAX)][int(T::Command::MAX)];
    vector<typename T::TimingEntry> full_crow_hit_full_restore_timing[int(T::Level::MAX)][int(T::Command::MAX)];
    vector<typename T::TimingEntry> crow_copy_timing[int(T::Level::MAX)][int(T::Command::MAX)];

	float trcd_crow_partial_hit = 1.0f, trcd_crow_full_hit = 1.0f;
	
	float tras_crow_partial_hit_partial_restore = 1.0f, tras_crow_partial_hit_full_restore = 1.0f;
	float tras_crow_full_hit_partial_restore = 1.0f, tras_crow_full_hit_full_restore = 1.0f;
	float tras_crow_copy_partial_restore = 1.0f, tras_crow_copy_full_restore = 1.0f; 
	
	float twr_partial_restore = 1.0f, twr_full_restore = 1.0f;

    map<int, int> row_act_hist[8][128]; // hardcoded for 8 bank and 128 SAs per bank (assuming 512-row SAs and 64K rows per bank)
    
};

template <>
vector<int> Controller<SALP>::get_addr_vec(
    SALP::Command cmd, list<Request>::iterator req);

template <>
bool Controller<SALP>::is_ready(list<Request>::iterator req);

template <>
void Controller<SALP>::initialize_crow_timing(vector<SALP::TimingEntry> timing[]
        [int(SALP::Command::MAX)], const float trcd_factor, const float tras_factor, 
        const float twr_factor, const float tfaw_factor);

//template <>
//void Controller<SALP>::update_crow_table_inv_index();

template <>
void Controller<ALDRAM>::update_temp(ALDRAM::Temp current_temperature);

//template <>
//void Controller<TLDRAM>::tick();

} /*namespace ramulator*/

#endif /*__CONTROLLER_H*/
