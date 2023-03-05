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
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <netdb.h>
#include <semaphore.h>
#include <sys/ipc.h>  
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <filesystem>

using namespace std;

#define MAX_CLIENTS 31
#define QLENS 30

void run(int me);
int passiveTCP(const char* service, int qlen);
int passivesock(const char *service, const char *protocol, int qlen);
void welcome(int n);
void broadcast_login(int n);
void create_pipe(vector <string> single_job, map <int, pair <int, int> > &pipe_map, int ID, string input);
vector<string> command_split(string command);
void parse(string str1, queue<string> &q);
void close_pipe(pair <int, int> p);
void execvp_work(string final_job, pair <int, int> left_pipe_id, pair <int, int> right_pipe_id, bool send_error_to_other, bool file_redirection, string redirection_file_name, bool number_pipe, int ID, int to_which_client, int from_which_client);
int get_to_client(string str);
int get_from_client(string str);
string del_redir_client_info(string str);
void who(int me);
void yell(int me, string yell_msg);
void tell(int me, int tellwho, string tell_msg);
void change_name(int me, string name_msg);
void broadcast_msg(string send_pipe_msg);
void check_from_client(int &from_which_client, int ID, string input);
void check_to_client(int &to_which_client, int ID, string input);
void logout(int ID);
void my_handler(int signo);
void write_shm_msg(string shm_msg, int n);
void write_fifo(int to_which_client);
void read_fifo(int from_which_client);

class user_info {
public:
    int ID;
    char name[21];
    char ip[25];
    int port;
    pid_t pid;
    char msg[1025];
    sem_t w_semaphore;
    sem_t userpipe_semaphore;
    int fd;
    int from_this_client[MAX_CLIENTS]; //from whom
    int now_to_you;
};

user_info *shm_addr;
int myid;
int job_cnt = 0;
map<int, pair<int, int> > pipe_map;
int r_fd[31];
int shmid;

int main(int argc, const char* argv[]) {
    cout << std::filesystem::current_path() << endl;
    mkdir("user_pipe", 0777);
    shmid = shmget(IPC_PRIVATE, sizeof(user_info) * 31, IPC_CREAT | 0666);
    shm_addr = (user_info *)shmat(shmid, NULL, 0);

    // share memory initial
	for(int i = 0; i < MAX_CLIENTS; ++i) {
		shm_addr[i].ID = -1;
        memset(shm_addr[i].name, '\0', 21);
		strcpy(shm_addr[i].name, "(no name)");
        memset(shm_addr[i].ip, '\0', 25);
		shm_addr[i].port = -1;
		shm_addr[i].pid = -1;
		memset(shm_addr[i].msg, '\0', 1025);
		sem_init(&shm_addr[i].w_semaphore, 1, 1);
		sem_init(&shm_addr[i].userpipe_semaphore, 1, 1);
        // cout << i << ":" << ret << endl;
        shm_addr[i].fd = -1;
        shm_addr[i].now_to_you = -1;
        for (int j = 0; j < MAX_CLIENTS; ++j){
            shm_addr[i].from_this_client[j] = -1;
        }
        r_fd[i] = -1;
	}

    int msock = passiveTCP(argv[1], QLENS);
    cout << "master sock number:" << msock << endl;
    while(1){
        setenv("PATH", "bin:.", 1);
        int ssock;
        struct sockaddr_in addr;
        int addrlen = sizeof(addr);
        ssock = accept(msock, (struct sockaddr *)&addr, (socklen_t*)&addrlen);
        cout << "ssock:" << ssock << endl;
        if(ssock < 0) {
            continue;
        }
        cout << "accept" << endl;
        int me;
        for(int i = 1; i < MAX_CLIENTS; ++i) {
            // initial this process info
            if(shm_addr[i].fd == -1) {
                shm_addr[i].fd = ssock;
                shm_addr[i].ID = i;
                strcpy(shm_addr[i].name, "(no name)");
                strcpy(shm_addr[i].ip, inet_ntoa(addr.sin_addr));
                shm_addr[i].port = ntohs(addr.sin_port);
                memset(shm_addr[i].msg, '\0', 1025);
                myid = i;
                break;
            }
            else if(i == MAX_CLIENTS - 1) {
                close(ssock);
            }
        }	
        if(signal(SIGUSR1, my_handler) == SIG_ERR) {
            //設定出錯
            // perror("Can't set handler for SIGUSR1\n");
            exit(1);
        }
        if(signal(SIGUSR2, my_handler) == SIG_ERR) {
            //設定出錯
            // perror("Can't set handler for SIGUSR2\n");
            exit(1);
        }
        if(signal(SIGINT, my_handler) == SIG_ERR) {
            //設定出錯
            // perror("Can't set handler for SIGUSR2\n");
            exit(1);
        }
        int fork_pid;
		while((fork_pid = fork()) < 0);
        if (fork_pid == 0){ 
            shm_addr[myid].pid = getpid();
            setenv("PATH", "bin:.", 1);
            welcome(myid);
            broadcast_login(myid);
            string prompt = "% ";
            write_shm_msg(prompt, myid);
            run(myid);
        }
        else if (fork_pid > 0){
            close(ssock);
            signal(SIGCHLD,SIG_IGN);
        }
    }
    close(msock);
}

void run(int me){
    while(1){
        //檢查別人有沒有給我東西
        int readlen;
        char input_array[15001];
        memset(input_array, '\0', 15001);
        readlen = read(shm_addr[me].fd, input_array, 15000);
        if (readlen < 0){
            continue;
        }
        else if (readlen == 0){
            //logout
            logout(me);
            shmdt(shm_addr);
            exit(0);
        }
        else {
            char *bp;
            if((bp = strchr(input_array, '\n')) != NULL)
                *bp = '\0';
            if((bp = strchr(input_array, '\r')) != NULL)
                *bp = '\0';
        }
        string input = string(input_array);
        cout << "input:" << input << endl;
        if (input == "exit"){
            logout(me);
            shmdt(shm_addr);
            exit(0);
        }
        queue<string> job_queue;
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
            for (int k = 0; k < work_vec.size(); k++) {
                // cout << work_vec[k] << endl;
                if (work_vec[k][0] == '|' || work_vec[k][0] == '!' || work_vec[k] == ">") {
                    single_job.push_back(tmp_str);
                    single_job.push_back(work_vec[k]);
                    tmp_str = "";
                }
                else {
                    tmp_str += work_vec[k];
                    tmp_str += " ";
                }
            }
            if (tmp_str != "")
                single_job.push_back(tmp_str);
            create_pipe(single_job, pipe_map, me, input);
        }
        //給我自己%
        string prompt = "% ";
        write_shm_msg(prompt, me);
    }
}

void create_pipe(vector <string> single_job, map <int, pair <int, int> > &pipe_map, int ID, string input){
    pair <int, int> left_pipe_id;
    pair <int, int> right_pipe_id;
    left_pipe_id.first = -1;
    left_pipe_id.second = -1;
    right_pipe_id.first = -1;
    right_pipe_id.second = -1;
    bool send_error_to_other = false;
    bool file_redirection = false;
    bool number_pipe = false;
    bool receive = false;
    string redirection_file_name = "";
    for (int i = 0; i < single_job.size(); i++) {
        int to_which_client = 0;
        int from_which_client = 0;
        cout << "single_job:" << single_job[i] << endl;
        // cout << "job_cnt:" << job_cnt << endl;
        // if (i == 0){
        if (pipe_map.find(job_cnt) != pipe_map.end()) {
            // cout << "someone sent msg to you" << job_cnt<< endl;
            receive = true;
            left_pipe_id.first = pipe_map[job_cnt].first;
            left_pipe_id.second = pipe_map[job_cnt].second;
            pipe_map.erase(job_cnt);
        }
        // }
        bool next = false;
        if (i != single_job.size()-1)    
            next = true;
        vector <string> env_vec;
        env_vec = command_split(single_job[i]);
        if (env_vec[0] == "printenv") {
            char *env_str = getenv(env_vec[1].c_str());
            if (env_str != NULL) 
                write_shm_msg(string(env_str) + "\n", ID);
        }
        else if (env_vec[0] == "setenv") {
            setenv(env_vec[1].c_str(), env_vec[2].c_str(), 1);
        }
        else if (env_vec[0] == "who" && env_vec.size() == 1) {
            who(ID);
        }
        else if (env_vec[0] == "yell") {
            yell(ID, input);
            break;
        }
        else if (env_vec[0] == "tell"){
            int tellwho = stoi(env_vec[1]);
            if (shm_addr[tellwho].fd == -1){
                string err_msg =  "*** Error: user #" + to_string(tellwho) + " does not exist yet. ***\n";
                write_shm_msg(err_msg, ID);
            }
            else tell(ID, tellwho, input);
            break;
        }
        else if (env_vec[0] == "name") {
            // input += " ";
            // cout << "name:" << env_vec[0] << endl;
            change_name(ID, input);
            break;
        }
        else if (i%2 == 0) {
            if (next == true && single_job[i+1] == "|") {
                int p[2];
                if (pipe(p) < 0) {
                    // cout << "pipe failed" << endl;
                    exit(-1);
                }
                if (i == 0) {
                    right_pipe_id.first = p[0];
                    right_pipe_id.second = p[1];
                }
                else if (i != single_job.size()-1){
                    // cout << "middle child" << endl;
                    left_pipe_id = right_pipe_id;
                    right_pipe_id.first = p[0];
                    right_pipe_id.second = p[1];
                }  
            }
            else if (i == single_job.size()-1) {
                if (single_job.size() != 1) {
                    left_pipe_id = right_pipe_id;
                    right_pipe_id.first = -1;
                    right_pipe_id.second = -1;
                }
            }
            else if (single_job[i+1] == ">") {
                if (i != 0) {
                    left_pipe_id = right_pipe_id;
                    right_pipe_id.first = -1;
                    right_pipe_id.second = -1;
                }
                file_redirection = true;
                redirection_file_name = single_job[i+2];
                redirection_file_name.pop_back();
                cout << "file name :" << redirection_file_name << endl;
            }
            else {
                if (single_job[i+1][0] == '|' || single_job[i+1][0] == '!') {
                    // cout << "number pipe is: " << single_job[i+1] << endl;
                    number_pipe = true;
                    if (single_job[i+1][0] == '!') 
                        send_error_to_other = true;                    
                    int pass_line;
                    stringstream ss;
                    ss << single_job[i+1].substr(1);
                    ss >> pass_line;
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
                            // cout << "pipe failed" << endl;
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
                }
                
            }
            // file_redirection = false;
            from_which_client = get_from_client(single_job[i]);
            to_which_client = get_to_client(single_job[i]);
            if (from_which_client != 0 || to_which_client != 0){
                single_job[i] = del_redir_client_info(single_job[i]);
            }
            if (from_which_client != 0)
                check_from_client(from_which_client, ID, input);
            if (to_which_client != 0)
                check_to_client(to_which_client, ID, input); 
            execvp_work(single_job[i], left_pipe_id, right_pipe_id, send_error_to_other, file_redirection, redirection_file_name, number_pipe, ID, to_which_client, from_which_client);
            send_error_to_other = false;
            number_pipe = false;
            redirection_file_name = "";
            receive = false;
        }     
        if (file_redirection == true)
            break;
    }
}

void execvp_work(string final_job, pair <int, int> left_pipe_id, pair <int, int> right_pipe_id, bool send_error_to_other, bool file_redirection, string redirection_file_name, bool number_pipe, int ID, int to_which_client, int from_which_client) {
    pid_t pid;
    int wait_status;
    pid = fork();
    while (pid < 0) {
        wait(&wait_status);
        pid = fork();
    }
    if (pid == 0) {
        cout << "l1:" << left_pipe_id.first << " l2:" << left_pipe_id.second << endl;
        cout << "r1:" << right_pipe_id.first << " r2:" << right_pipe_id.second << endl;
        cout << "to = " << to_which_client << ", from = " << from_which_client << endl;
        if (send_error_to_other == true) {
            // cout << "send_error_to_other == true" << endl;
            dup2(right_pipe_id.second, STDERR_FILENO);
        }
        else if (send_error_to_other == false) {
            // cout << "send_error_to_other == false" << endl;
            close(STDERR_FILENO);
            dup2(shm_addr[ID].fd, STDERR_FILENO);
        }
        
        if ((left_pipe_id.first == left_pipe_id.second) && (right_pipe_id.first == right_pipe_id.second)) {
            //ex: ls
            // cout << "ID:" << ID << endl;
            close(STDOUT_FILENO); // Close stdout
            close(STDERR_FILENO);
            dup2(shm_addr[ID].fd, STDOUT_FILENO);
            dup2(shm_addr[ID].fd, STDERR_FILENO);
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
            close(STDOUT_FILENO); // close stdout
            close(STDERR_FILENO);
            dup2(shm_addr[ID].fd, STDOUT_FILENO);
            dup2(shm_addr[ID].fd, STDERR_FILENO);
        }
        else {
            // cout << "middle in execvp_work" << endl;
            dup2(left_pipe_id.first, STDIN_FILENO);
            dup2(right_pipe_id.second, STDOUT_FILENO);
            close_pipe(left_pipe_id);
            close_pipe(right_pipe_id);
        }

        if (file_redirection == true) {
            //file redirection
            freopen(redirection_file_name.c_str(), "w", stdout);
        }

        if (to_which_client == -1)
            freopen("/dev/null", "w", stdout);
        else if (to_which_client != 0){
            //connect to fifo
            string write_fifo_path = "user_pipe/fifo." + to_string(myid) + "_" + to_string(to_which_client);
            int w_fd = open(write_fifo_path.c_str(), O_WRONLY);
            // cout << "w_fd = " << w_fd << endl;
            dup2(w_fd, STDOUT_FILENO);
            close(w_fd);
        }
        if (from_which_client == -1)
            freopen("/dev/null", "r", stdin);
        else if (from_which_client != 0){
            //connect to fifo
            // int r_fd = open(read_fifo_path.c_str(), O_RDONLY|O_NONBLOCK);
            dup2(r_fd[from_which_client], STDIN_FILENO);
            close(r_fd[from_which_client]);
        }

        vector <string> tmp_vec;
        tmp_vec = command_split(final_job);
        char** arg;
        arg = new char* [tmp_vec.size()+1];
        for (int j = 0; j < tmp_vec.size(); j++) {
            arg[j] = (char*)tmp_vec[j].c_str();
        }
        arg[tmp_vec.size()] = NULL;
        shmdt(shm_addr);
        
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
            cerr << "Unknown command: [" << unknown_command << "]." << endl;
            exit(0);
        }
    }
    else {
        cout << "number pipe: " << number_pipe << endl;
        if (from_which_client > 0)
            close(r_fd[from_which_client]);
        if (right_pipe_id.first != right_pipe_id.second) {
            if (left_pipe_id.first != left_pipe_id.second) {
                close_pipe(left_pipe_id);
                cout << "parent of middle" << endl;
            }
            else {
                cout << "parent of first" << endl;
            }
            signal(SIGCHLD,SIG_IGN);
        }
        else {
            // last child
            if (left_pipe_id.first != left_pipe_id.second)
                close_pipe(left_pipe_id);
            cout << "parent of last" << endl;
            if (to_which_client > 0){ 
                // cout << "p to someone" << endl;
                signal(SIGCHLD,SIG_IGN);
            }
            else if (number_pipe != true){
                cout << "not number" << endl;
                wait(NULL);
            }
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

void broadcast_msg(string send_pipe_msg){
    for (int i = 1; i < MAX_CLIENTS; ++i) {
        if (shm_addr[i].fd != -1) {
            write_shm_msg(send_pipe_msg, i);
        }
    }
}

void broadcast_login(int n) {
    string s = "*** User '";
    s += shm_addr[n].name;
    s += "' entered from ";
    s += string(shm_addr[n].ip) + ":" + to_string(shm_addr[n].port);
    s += ". ***\n";

    for (int i = 1; i < MAX_CLIENTS; ++i) {
        if (shm_addr[i].fd != -1) {
            write_shm_msg(s, i);
        }
    }
}
//1 kill跟2說了，他覺得他的工作結束了所以他就會想要去write fifo，所以他開始等2的semaphore，但這會失敗因為2還沒有接受！
void write_shm_msg(string shm_msg, int n){
    sem_wait(&shm_addr[n].w_semaphore);
    // cout << "write_shm_msg wait " << n << " sem" << endl;
    strcpy(shm_addr[n].msg, shm_msg.c_str());
    kill(shm_addr[n].pid, SIGUSR1);	
}

void my_handler(int signo){
    if (signo == SIGINT){
        //ctrl-c, please clear shared memory
        shmdt(shm_addr);
        shmctl(shmid, IPC_RMID, 0);
        exit(0);
    }
    else if (signo == SIGUSR1){
        //someone write into your share memory msg
        // cout << myid << " signo = SIGUSR1" << endl;
        string my_msg = string(shm_addr[myid].msg);
        send(shm_addr[myid].fd, my_msg.c_str(), strlen(my_msg.c_str()), 0);
        memset(shm_addr[myid].msg, '\0', 1025);
        sem_post(&shm_addr[myid].w_semaphore);
        // int val;
        // sem_getvalue(&shm_addr[myid].w_semaphore, &val);
        // cout << "i am " << myid << ", my sem is " << val << endl;
    }
    else if (signo == SIGUSR2){
        //someone sent you through fifo
        // cout << "handler mkfifo" << endl;
        string myfifo_path = "user_pipe/fifo." + to_string(shm_addr[myid].now_to_you) + "_" + to_string(myid);
        mkfifo(myfifo_path.c_str(), 0666);
        // cout << "before read" << endl;
        int fd = open(myfifo_path.c_str(), O_RDONLY);
        // cout << "fd = " << fd << endl;
        r_fd[shm_addr[myid].now_to_you] = fd;
        sem_post(&shm_addr[myid].userpipe_semaphore);
    }
}

int get_to_client(string str){
    string s = "";
    vector<string> vec = command_split(str);
    for (int i = 0; i < vec.size(); i++) {
        // cout << vec[i];
        if (vec[i][0] == '>') {
            s = vec[i].substr(1);
            break;
        }
    }
    if (s == "")
        return 0;
    else return stoi(s);
}

int get_from_client(string str){
    string s = "";
    string new_str = "";
    vector<string> vec = command_split(str);
    for (int i = 0; i < vec.size(); i++) {
        if (vec[i][0] == '<') {
            s = vec[i].substr(1);
            break;
        }
    }
    if (s == "")
        return 0;
    else return stoi(s);
}

string del_redir_client_info(string str){
    string new_str = "";
    for (int i = 0; i < str.length(); i++) {
        if (str[i] == '>' || str[i] == '<') 
            break;
        else new_str += str[i];
    }
    return new_str;
}

void write_fifo(int to_which_client){
    cout << "writer try wait" << endl;
    // int val;
    // sem_getvalue(&shm_addr[to_which_client].w_semaphore, &val);
    // cout << val << endl;
    sem_wait(&shm_addr[to_which_client].userpipe_semaphore);
    // cout << "sem_wait ok" << endl;
    shm_addr[to_which_client].now_to_you = myid;
    shm_addr[to_which_client].from_this_client[myid] = 1;
    // sem_post(&shm_addr[to_which_client].w_semaphore);
    // cout << "start kill" << endl;
    kill(shm_addr[to_which_client].pid, SIGUSR2);
    // cout << "writer start mkfifo" << endl;
    string myfifo_path = "user_pipe/fifo." + to_string(myid) + "_" + to_string(to_which_client);
    mkfifo(myfifo_path.c_str(), 0666);
    cout << "write_fifo end" << endl;
}

void read_fifo(int from_which_client){
    cout << "reader try wait" << endl;
    sem_wait(&shm_addr[myid].userpipe_semaphore);//ok
    cout <<"read_fifo wait " << myid << " sem" << endl;
    shm_addr[myid].from_this_client[from_which_client] = -1;
    sem_post(&shm_addr[myid].userpipe_semaphore);
    cout << "read_fifo end" << endl;
}

void who(int me){
    string str = "<ID>    <nickname>    <IP:port>    <indicate me>\n";   
    for (int i = 1; i < MAX_CLIENTS; i++){
        if (shm_addr[i].fd != -1){  
            str += (to_string(shm_addr[i].ID) + "    " + string(shm_addr[i].name) + "    " + string(shm_addr[i].ip) + ":" + to_string(shm_addr[i].port)+ "    ");
            if (me == shm_addr[i].ID)
                str += "<-me";
            str += "\n";
        }
    }
    write_shm_msg(str, me);
}

void tell(int me, int tellwho, string tell_msg){
    int space = 2;
    string str = "*** " + string(shm_addr[me].name) + " told you ***: ";
    for (int i = 0; i < tell_msg.length(); ++i){
        if(space == 0)
            str += tell_msg[i];
        else if (tell_msg[i] == ' ') space--;
    }
    str += "\n";
    write_shm_msg(str, tellwho);
}

void yell(int me, string yell_msg) {
    string str = "*** " + string(shm_addr[me].name) + " yelled ***:" + yell_msg.substr(4) + "\n";
    for (int i = 1; i < MAX_CLIENTS; ++i){
        if (shm_addr[i].fd != -1)
            write_shm_msg(str, i);
    }
}

void change_name(int me, string name_msg){
    string del_space_name = "";
    name_msg = name_msg.substr(5, name_msg.length()-5);
    for (int k = 0; k < name_msg.length(); k++){
        if (name_msg[k] != ' ')
            del_space_name += name_msg[k];
        else break;
    }
    name_msg = del_space_name;
    string str = "";
    for (int i = 1; i < MAX_CLIENTS; i++){
        if (name_msg == string(shm_addr[i].name)){
            cout << "test" << endl;
            str = "*** User '" + name_msg + "' already exists. ***\n";
            write_shm_msg(str, me);
            return;
        }
    }
    str = "*** User from " + string(shm_addr[me].ip) + ":" + to_string(shm_addr[me].port) + " is named '" + name_msg + "'. ***\n";
    for (int i = 1; i < MAX_CLIENTS; ++i){
        if (shm_addr[i].fd != -1)
            write_shm_msg(str, i);
    }
    strcpy(shm_addr[me].name, name_msg.c_str());
}

void logout(int n){
    // FD_CLR(shm_addr[n].fd, &afds);
    string logout_msg = "*** User '" + string(shm_addr[n].name) + "' left. ***\n";
    close(shm_addr[n].fd);
    shm_addr[n].ID = -1;
    shm_addr[n].fd = -1;
    shm_addr[n].port = -1;
    shm_addr[n].pid = -1;
    for (int i = 1; i < MAX_CLIENTS; i++)
        shm_addr[n].from_this_client[i] = -1;
    memset(shm_addr[n].name, '\0', 21);
    strcpy(shm_addr[n].name, "(no name)");
    memset(shm_addr[n].ip, '\0', 25);
    memset(shm_addr[n].msg, '\0', 1025);
    sem_init(&shm_addr[n].w_semaphore, 1, 1);
    sem_init(&shm_addr[n].userpipe_semaphore, 1, 1);
    shm_addr[n].now_to_you = -1;
    broadcast_msg(logout_msg);
}
 

void welcome(int n) {
    string welcome_msg = "****************************************\n** Welcome to the information server. **\n****************************************\n";
    write_shm_msg(welcome_msg, n);
}

void check_from_client(int &from_which_client, int ID, string input) {
    if (from_which_client > 30 || shm_addr[from_which_client].fd == -1) {
        string err_msg =  "*** Error: user #" + to_string(from_which_client) + " does not exist yet. ***\n";
        write_shm_msg(err_msg, ID);
        from_which_client = -1;
        // return true;
    }
    //檢查這個人有沒有傳東西給你
    else if (shm_addr[myid].from_this_client[from_which_client] == -1){
        string err_msg = "*** Error: the pipe #" + to_string(from_which_client) + "->#" + to_string(ID) + " does not exist yet. ***\n";
        write_shm_msg(err_msg, ID);
        from_which_client = -1;
        // return true;
    }
    else {
        string public_recv_msg = "*** " + string(shm_addr[ID].name) + " (#" + to_string(ID) + ") just received from " + string(shm_addr[from_which_client].name) + " (#" + to_string(from_which_client) + ") by '" + input + "' ***\n";
        broadcast_msg(public_recv_msg);
        //去fifo拿東西
        read_fifo(from_which_client);
        // string public_recv_msg = "*** " + string(shm_addr[ID].name) + " (#" + to_string(ID) + ") just received from " + string(shm_addr[from_which_client].name) + " (#" + to_string(from_which_client) + ") by '" + input + "' ***\n";
        // return false;
    }
}

void check_to_client(int &to_which_client, int ID, string input) {
    if (to_which_client > 30 || shm_addr[to_which_client].fd == -1) {
        string err_msg =  "*** Error: user #" + to_string(to_which_client) + " does not exist yet. ***\n";
        write_shm_msg(err_msg, ID);
        to_which_client = -1;
        // return true;
    }
    //檢查上次傳給這個人他收了沒
    else if (shm_addr[to_which_client].from_this_client[myid] != -1){
        string err_msg = "*** Error: the pipe #" + to_string(ID) + "->#" + to_string(to_which_client) + " already exists. ***\n";
        write_shm_msg(err_msg, ID);
        to_which_client = -1;
        // return true;
    }
    else {
        string public_send_msg = "*** " + string(shm_addr[ID].name) +  " (#" + to_string(ID) +") just piped '" + input +  "' to " + string(shm_addr[to_which_client].name) +  " (#" + to_string(to_which_client) + ") ***\n";
        broadcast_msg(public_send_msg);
        //把東西放進fifo
        write_fifo(to_which_client);
        // return false;
    }
}

int passiveTCP(const char* service, int qlen) {
    return passivesock(service, "tcp", qlen);
}

int passivesock(const char *service, const char *protocol, int qlen) {
    struct servent *pse;            // pointer to service info entry
    struct protoent *ppe;           // pointer to protocol info entry
    struct sockaddr_in sin;         // an endpoint addr
    int sock, type;                 // socketfd & socket type

    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    
    //map service name to port number
    if(pse = getservbyname(service, protocol))
        sin.sin_port = htons(ntohs((u_short) pse -> s_port));
    else if((sin.sin_port = htons((u_short)atoi(service))) == 0) {
        exit(-1);
    }

    //map protocol name to protocol number
    if((ppe = getprotobyname(protocol)) == 0) { 
        exit(-1);
    }
    
    //use protocol to choose a socket type
    if(strcmp(protocol, "tcp") == 0)
        type = SOCK_STREAM;
    else
        type = SOCK_DGRAM;

    // allocate a socket
    sock = socket(PF_INET, type, ppe->p_proto);
    if(sock < 0) {
        exit(-1);
    }
    const int enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    //bind the socket
    if(bind(sock, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
        exit(-1);
    }
    if(type == SOCK_STREAM && listen(sock, qlen) < 0) {
        exit(-1);
    }
    return sock;
}
