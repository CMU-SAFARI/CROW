#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <vector>

using namespace std;

vector<string> split(const char *str, char c = ' ')
{
    vector<string> result;

    do
    {
        const char *begin = str;

        while(*str != c && *str)
            str++;

        result.push_back(string(begin, str));
    } while (0 != *str++);

    return result;
}


int main(int argc, char** argv){

	if(argc < 4 || argc > 6) {
        cout << "Usage: sparser {file_to_parse} {cell_charge: h/l} {VDD} [tRAS threshold - optional]" << endl;
        return -1;
    }
    
    fstream myfile;
	myfile.open (argv[1]);	

	if(!myfile.is_open()){
		cout << "Error on opening the file: " << argv[1] << endl;
		return -1;
	}

	//process the file
	string line;

	float min_trcd=500000, max_trcd=0;
	float min_tras=500000, max_tras=0;
	float min_twr=500000, max_twr=0;
	int num_success_trcd = 0;
	int num_success_tras = 0;
	int num_success_twr = 0;

	bool dir = (argv[2][0] == 'h'); // indicates the value the bitline should reach
                                    // h for high (logic-1)
                                    
    float VDD = atof(argv[3]);

    float TRAS_THRESHOLD = 0.93f;
    if(argc == 5)
        TRAS_THRESHOLD = atof(argv[4]);
    else
        TRAS_THRESHOLD = 0.93f;

    const float TRCD_THRESHOLD = 0.75f;
    const float SKIP_TIME = 0.000000020f;
    const float WRITE_TIME = 0.000000120f; // make sure to modify this if you change the write logic enable time in the SPICE model
    string BITLINE_NAME = "bitline0";

    string CELL_NAME = "mb0";
    if(argc == 6)
        CELL_NAME = argv[5];
	
	bool skip_run = true;
	bool skip_trcd = true;
    bool skip_tras = true;
	getline(myfile, line); //skip the first redundant line

    // find the order of the BITLINE_NAME
    getline(myfile, line);
    vector<string> tokens = split(line.c_str(), '\t');
    int var_ind = 0;

    for(string s : tokens) {
        if(s.compare(BITLINE_NAME))
            var_ind++;
        else
            break;
    }
        
    if(var_ind >= tokens.size()) {
        cout << "Error! Variable: " << BITLINE_NAME << "not found!" << endl;
        return -2;
    }

    int cell_var_ind = 0;

    for(string s : tokens) {
        if(s.compare(CELL_NAME))
            cell_var_ind++;
        else
            break;
    }


    if(cell_var_ind >= tokens.size()) {
        cout << "Error! Variable: " << CELL_NAME << "not found!" << endl;
        return -2;
    }

	while(getline(myfile, line)){
        vector<string> tokens = split(line.c_str(), '\r');
        float cur_time = stof(tokens[0]);

		//check if it is a start of a new run
		if(stof(tokens[0]) == 0.0f){
			skip_run = false;
            skip_trcd = false;
            skip_tras = false;
			continue;	
		}

		if(skip_run)
			continue;

		//skip until 20ns
		if(cur_time < SKIP_TIME)
			continue;

		float bitline_value = stof(tokens[var_ind]);
		float cell_value = stof(tokens[cell_var_ind]);

		if(dir){
			if(!skip_trcd && (bitline_value >= VDD*TRCD_THRESHOLD)){
				if(cur_time < min_trcd)
					min_trcd = cur_time;
				if(cur_time > max_trcd)
					max_trcd = cur_time;

				skip_trcd = true;
				num_success_trcd++;
			}

            if((skip_trcd && !skip_tras) && (cell_value >= VDD*TRAS_THRESHOLD)){
                if(cur_time < min_tras)
                    min_tras = cur_time;
                if(cur_time > max_tras)
                    max_tras = cur_time;

                skip_tras = true;
                num_success_tras++;
            }

            if(skip_tras && (cell_value <= VDD*(1.0f - TRAS_THRESHOLD))) {
                if(cur_time < min_twr)
                    min_twr = cur_time;
                if(cur_time > max_twr)
                    max_twr = cur_time;

                skip_run = true;
                num_success_twr++;
            }

		} else{
            if(!skip_trcd && (bitline_value <= VDD*(1.0f - TRCD_THRESHOLD))){
				if(cur_time < min_trcd)
					min_trcd = cur_time;
				if(cur_time > max_trcd)
					max_trcd = cur_time;

				skip_trcd = true;
				num_success_trcd++;
			}

            if((skip_trcd && !skip_tras) && (cell_value <= VDD*(1.0f - TRAS_THRESHOLD))){
                if(cur_time < min_tras)
                    min_tras = cur_time;
                if(cur_time > max_tras)
                    max_tras = cur_time;

                skip_tras = true;
                num_success_tras++;
            }

            if(skip_tras && (cell_value >= VDD*TRAS_THRESHOLD)) {
                if(cur_time < min_twr)
                    min_twr = cur_time;
                if(cur_time > max_twr)
                    max_twr = cur_time;

                skip_run = true;
                num_success_twr++;
            }
			
		}

	}

	cout << "min_trcd, " << (min_trcd-SKIP_TIME)*1000000000 << endl;
    cout << "max_trcd, " << (max_trcd-SKIP_TIME)*1000000000 << endl;
    cout <<"num_correct_trcd, " << num_success_trcd << endl;

    cout << "min_tras, " << (min_tras-SKIP_TIME)*1000000000 << endl;
    cout << "max_tras, " << (max_tras-SKIP_TIME)*1000000000 << endl;
    cout << "num_correct_tras, " << num_success_tras << endl;

    cout << "min_twr, " << (min_twr-SKIP_TIME-WRITE_TIME)*1000000000 << endl;
    cout << "max_twr, " << (max_twr-SKIP_TIME-WRITE_TIME)*1000000000 << endl;
    cout << "num_correct_twr, " << num_success_twr << endl;


	myfile.close();
	
	return 0;
}
