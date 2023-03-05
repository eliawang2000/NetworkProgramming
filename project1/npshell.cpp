#include <iostream>
#include <string>
#include <unistd.h>
#include <cstring>
#include <string.h>
#include <vector>
#include <signal.h>
#include <sstream>
#include <queue>
#include <map>
#include <utility>
#include <sys/wait.h>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
using namespace std;

void parse(string str, queue<string> &q);
vector<string> command_split(string command);
void create_pipe(vector <string> single_job, int job_cnt, map <int, pair <int, int> > &pipe_map);
void close_pipe(pair <int, int> p);
vector<string> split_work(string str);
void execvp_work(string final_job, pair <int, int> left_pipe_id, pair <int, int> right_pipe_id, bool send_error_to_other, bool file_redirection, string redirection_file_name, bool number_pipe);

int main() {
    setenv("PATH", "bin:.", 1);
    queue<string> job_queue;
    map<int, pair<int, int> > pipe_map;
    int job_cnt = 0;
    signal(SIGCHLD, SIG_IGN);
    while (1) { //get commands
        cout << "% ";
        string input;
        getline(cin, input); 
        if (cin.eof())
            exit(0);
        if (input == "exit"){
            exit(0);
        }
        parse(input, job_queue);
        while (job_queue.size() != 0) {
            string command;
            command = job_queue.front();
            // cout << "command:" << command << endl;
            job_queue.pop();
            job_cnt++;
            vector<string> work_vec;
            vector<string> single_job;
            string tmp_str = "";
            work_vec = command_split(command);
            for (int i = 0; i < work_vec.size(); i++) {
                if (work_vec[i][0] == '|' || work_vec[i][0] == '!' || work_vec[i][0] == '>') {
                    single_job.push_back(tmp_str);
                    single_job.push_back(work_vec[i]);
                    tmp_str = "";
                }
                else {
                    tmp_str += work_vec[i];
                    tmp_str += " ";
                }
            }
            if (tmp_str != "")
                single_job.push_back(tmp_str);
            create_pipe(single_job, job_cnt, pipe_map);
        }
    }
}

void create_pipe(vector <string> single_job, int job_cnt, map <int, pair <int, int> > &pipe_map){
    bool receive = false;
    pair <int, int> left_pipe_id;
    pair <int, int> right_pipe_id;
    left_pipe_id.first = -1;
    left_pipe_id.second = -1;
    right_pipe_id.first = -1;
    right_pipe_id.second = -1;
    bool send_error_to_other = false;
    bool file_redirection = false;
    bool number_pipe = false;
    string redirection_file_name = "";
    // vector <string> single_command;
    for (int i = 0; i < single_job.size(); i++) {
        // cout << single_job[i] << endl;
        if (pipe_map.find(job_cnt) != pipe_map.end()) {
            // cout << "someone sent msg to job" << job_cnt<< endl;
            receive = true;
            left_pipe_id.first = pipe_map[job_cnt].first;
            left_pipe_id.second = pipe_map[job_cnt].second;
            // cout << "receive from:" << pipe_map[job_cnt].first << " " << pipe_map[job_cnt].second << endl;
            pipe_map.erase(job_cnt);
        }
        bool next = false;
        if (i != single_job.size()-1)    
            next = true;

        // cout << single_job[i] << endl;
        vector <string> env_vec;
        env_vec = command_split(single_job[i]);

        if (env_vec[0] == "printenv") {
            // cout << "printenv" << endl;
            // cout << env_vec[1] << "!" << endl;
            char *str = getenv(env_vec[1].c_str());
            if (str != NULL) 
                // cout << (stdout, str) << endl;
                fprintf(stdout, "%s\n", str);
        }
        else if (env_vec[0] == "setenv") {
            setenv(env_vec[1].c_str(), env_vec[2].c_str(), 1);
        }
        // else if (env_vec[0] == "exit") {
        //     exit(0);
        // }
        // else if(execvp(arg[0], arg) < 0) {
        //     single_job[i].pop_back();
        //     cout << "Unknown command: [" << single_job[i] << "]." << endl;
        // }
        else if (i%2 == 0) {
            if (next == true && single_job[i+1] == "|") {
                int p[2];
                if (pipe(p) < 0) {
                    cout << "pipe failed" << endl;
                    exit(-1);
                }
                if (i == 0) {
                    // cout << "first child:" << single_job[i] << endl;
                    // cout << "l:" << left_pipe_id.first << " " << left_pipe_id.second << endl;
                    right_pipe_id.first = p[0];
                    right_pipe_id.second = p[1];
                    // cout << "r:" << right_pipe_id.first << " " << right_pipe_id.second << endl;
                    // receive = false;
                }
                else if (i != single_job.size()-1){
                    // cout << "middle child:" << single_job[i] << endl;
                    left_pipe_id = right_pipe_id;
                    right_pipe_id.first = p[0];
                    right_pipe_id.second = p[1];
                }   
            }
            else if (i == single_job.size()-1) {
                // cout << "last child:" << single_job[i] << endl;
                if (single_job.size() != 1) {
                    // cout << "hi" << endl;
                    left_pipe_id = right_pipe_id;
                    right_pipe_id.first = -1;
                    right_pipe_id.second = -1;
                }
                
                // cout << "last child receive is:" << receive;
                // cout << "last: " << left_pipe_id.first << " " << left_pipe_id.second << endl;
            }
            else if (single_job[i+1] == ">") {
                if (i != 0) {
                    left_pipe_id = right_pipe_id;
                    right_pipe_id.first = -1;
                    right_pipe_id.second = -1;
                }
                // cout << "it's file redirection to " << single_job[i+2] << endl;
                // cout << single_job[i+2] << endl;
                file_redirection = true;
                // redirection_file_name += "\"";
                redirection_file_name = single_job[i+2];
                // redirection_file_name = redirection_file_name.strsub(0, redirection_file_name.length() - 1);
                redirection_file_name.pop_back();
                // redirection_file_name += "\"";
                // cout << "file name :" << redirection_file_name << endl;
                // i = single_job.size();
            }
            else {
                if (single_job[i+1][0] == '|' || single_job[i+1][0] == '!') {
                    // cout << "number pipe is: " << single_job[i+1] << endl;
                    number_pipe = true;
                    if (single_job[i+1][0] == '!') 
                        send_error_to_other = true;                    
                    // cout << "number pipe" << endl;
                    int pass_line;
                    stringstream ss;
                    ss << single_job[i+1].substr(1);
                    ss >> pass_line;
                    // cout << "pass line:" << pass_line << endl;
                    int target_id;
                    target_id = job_cnt + pass_line;
                    // cout << "job " << job_cnt << " sent to " << target_id << endl;
                    if (pipe_map.find(target_id) != pipe_map.end()) {
                        // cout << "you are not first one to send to target" << endl;
                        if (receive == false && i != 0)
                            left_pipe_id = right_pipe_id;
                        right_pipe_id.first = pipe_map[target_id].first;
                        right_pipe_id.second = pipe_map[target_id].second;
                    }
                    else {
                        int p[2];
                        if (pipe(p) < 0) {
                            cout << "pipe failed" << endl;
                            exit(-1);
                        }
                        // cout << "you are first one to send to target" << endl;
                        pipe_map[target_id].first = p[0];
                        pipe_map[target_id].second = p[1];
                        if (receive == false)
                            left_pipe_id = right_pipe_id;
                        right_pipe_id.first = p[0];
                        right_pipe_id.second = p[1];
                    }
                    // cout << "l:" << left_pipe_id.first << " " << left_pipe_id.first << endl;
                    // cout << "r:" << right_pipe_id.first << " " << right_pipe_id.second << endl;
                }
            }
            execvp_work(single_job[i], left_pipe_id, right_pipe_id, send_error_to_other, file_redirection, redirection_file_name, number_pipe);
            // file_redirection = false;
            send_error_to_other = false;
            number_pipe = false;
            redirection_file_name = "";
        }     
        if (file_redirection == true)
            break;
    }
}

void execvp_work(string final_job, pair <int, int> left_pipe_id, pair <int, int> right_pipe_id, bool send_error_to_other, bool file_redirection, string redirection_file_name, bool number_pipe) {
    pid_t pid;
    int wait_status;
    // cout << "send error to other:" << send_error_to_other << endl;
    if ((pid = fork()) < 0) {
        // cout << "fork child failed" << endl;
        wait(&wait_status);
        pid = fork();
    }
    if (pid == 0) {
        if (send_error_to_other == true) {
            dup2(right_pipe_id.second, STDERR_FILENO);
        }
        if (file_redirection == true) {
            //file redirection
            // cout << "execvp_file redirection" << endl;
            freopen(redirection_file_name.c_str(), "w", stdout);
        }
        if ((left_pipe_id.first == left_pipe_id.second) && (right_pipe_id.first == right_pipe_id.second)) {
            //ex: ls
            // cout <<"ex: ls" << endl;
        } 
        else if (left_pipe_id.first == left_pipe_id.second) {
            // cout << "first in execvp_work" << endl;
            dup2(right_pipe_id.second, STDOUT_FILENO);
            close_pipe(right_pipe_id);
        }
        else if (right_pipe_id.first == right_pipe_id.second){
            // cout << "last in execvp_work" << endl;
            dup2(left_pipe_id.first, STDIN_FILENO);
            close_pipe(left_pipe_id);
        }
        else {
            // cout << "middle in execvp_work" << endl;
            dup2(left_pipe_id.first, STDIN_FILENO);
            dup2(right_pipe_id.second, STDOUT_FILENO);
            close_pipe(left_pipe_id);
            close_pipe(right_pipe_id);
        }
        // }
        
        vector <string> tmp_vec;
        tmp_vec = command_split(final_job);
        char** arg;
        arg = new char* [tmp_vec.size()+1];
        for (int j = 0; j < tmp_vec.size(); j++) {
            arg[j] = (char*)tmp_vec[j].c_str();
        }
        arg[tmp_vec.size()] = NULL;
        // string path_env = "bin/";
        // char *path_str = &path_env[0];
        // char *path = strcat(path_str, arg[0]);
        // cout << "path = " << path << endl;
        execvp(arg[0], arg);
        if (execvp(arg[0], arg) < 0) {
            string unknown_command;
            unknown_command = "";
            for (int j = 0; j < final_job.size(); j++){
                if (final_job[j] != ' ') {
                    unknown_command += final_job[j];
                }
                else {
                    break;
                }
            }
            // final_job.pop_back();
            cout << "Unknown command: [" << unknown_command << "]." << endl;
            exit(0);
        }
    }
    else {
        if (right_pipe_id.first != right_pipe_id.second) {
            if (left_pipe_id.first != left_pipe_id.second) {
                close_pipe(left_pipe_id);
                // cout << "parent of middle" << endl;
            }
            else {
                // cout << "parent of first" << endl;
            }
            signal(SIGCHLD,SIG_IGN);
        }
        else {
            // last child
            if (left_pipe_id.first != left_pipe_id.second)
                close_pipe(left_pipe_id);
            // cout << "parent of last" << endl;
            if (number_pipe != true)
                wait(NULL);
            // cout << "finish" << endl;
        }
    }
}

void close_pipe(pair <int, int> p) {
    close(p.first);
    close(p.second);
}

vector<string> command_split(string command) {
    vector <string> command_splited;
    vector<string> arr;
    istringstream ss(command);
    string word;
    while(ss >> word) {
        arr.push_back(word);
    }
    return arr;
} 

void parse(string str1, queue<string> &q) {
    const char* d = " ";
    char *p;
    char str2[str1.size()+1];
    strcpy(str2, str1.c_str()); 

    p = strtok(str2, d);
    string tmp_str;
    tmp_str = "";
    while (p != NULL) {
        if ((p[0] == '|' || p[0] == '!') && (strlen(p) != 1)) { //number pipe
            tmp_str += p;
            q.push(tmp_str);
            tmp_str = "";
        }
        // else if (p[0] == '>') {
        //     //file redirection
        //     q.push(tmp_str);
        // }
        else {
            tmp_str += p;
            tmp_str += " ";
        }
        p = strtok(NULL, d);		   
    }
    if (tmp_str != "") {
        q.push(tmp_str);
    }
    return;
}


// vector <string> file_string;
//         ifstream ifs("1.txt");
//         if (!ifs.is_open()) {
//             cout << "Failed to open file.\n";
//         } 
//         else {
//             string s;
//             while (getline(ifs, s)) {
//                 cout << s << "\n";
//                 file_string.push_back(s);
//             }
//             ifs.close();
//         }
//         ifs.close();


