#include<iostream>
#include<strings.h> //bzero()
#include<sys/types.h>
#include<sys/socket.h> //socket(), bind(), listen(), accept()
#include<netinet/in.h> //struct sockaddr_in
#include<unistd.h> //read(), write()
#include<sstream> //stringstream
#include<vector>
#include<unordered_map>
#include<pthread.h>
#include<fstream>
#include<arpa/inet.h> //inet_pton()
#include<unordered_set>
#include<limits.h> //PATH_MAX
#include<stdlib.h>
#include<sys/stat.h> //stat()
#include<algorithm> //reverse()

using namespace std;

typedef unsigned long long int ulli;

//globals
const int BACKLOG = 10;
const int BUFFER_SIZE = 256;

ofstream fout;

class Peer
{
    public:
    string username, password, status, listening_port;

    Peer(string u, string pw, string p_no)
    {
        username = u;
        password = pw;
        listening_port = p_no;
        status = "out";
    }
};

unordered_map<string, Peer*> USERS_NAME; //username - peer-info map
unordered_map<string, Peer*> USERS_PN; //port_no - peer-info map

class File
{
    public:
    string name, path, owner_port;

    File(string n, string p, string o_p)
    {
        name = n;
        path = p;
        owner_port = o_p;
    }
};

class Group
{
    public:
    string id;
    Peer* owner;
    unordered_set<Peer*> members;
    unordered_set<Peer*> pending_list;
    unordered_set<File*> files;
    unordered_set<string> file_names;

    Group(string name, string owner_port)
    {
        id = name;
        owner = USERS_PN.find(owner_port)->second;
    }

    void add_to_pending(string port)
    {
        Peer* requester = USERS_PN.find(port)->second;
        pending_list.insert(requester);
    }

    void add_member(Peer* requester)
    {
        members.insert(requester);
    }

};

unordered_map<string, Group*> GROUPS; //group id - Group* map

void debug(string msg)
{
    fout<<msg<<endl;
}

void tokenize(string msg, vector<string>& tokens)
{
    tokens.clear();
    stringstream ss(msg);
    string token;
    while(ss>>token) tokens.push_back(token);
}

string create_user(string username, string password, string port_no)
{
    if(USERS_NAME.find(username) != USERS_NAME.end()) return "User already exists. Cannot create new user\n";
    Peer* new_user = new Peer(username, password, port_no);
    USERS_PN[port_no] = new_user;
    USERS_NAME[username] = new_user;
    string response = "New user details: "+ USERS_PN[port_no]->username + ", " + USERS_PN[port_no]->password + ", " + port_no + "\n";
    return response;
}

string send_message(string dest_name)
{
    string dest_port_no = USERS_NAME[dest_name]->listening_port;
    return dest_port_no;
}

string login(string username, string password)
{
    if(USERS_NAME.find(username) == USERS_NAME.end()) return "account doesn't exist. Try creating a new user\n";
    if(USERS_NAME[username]->status == "in") return "user already logged in\n";
    if(password == USERS_NAME[username]->password)
    {
        USERS_NAME[username]->status = "in";
        return "logged in\n";
    }
    else return "wrong password\n";
}

string logout(string port)
{
    if(USERS_PN.find(port) == USERS_PN.end()) return "Your account doesn't exist\n";
    if(USERS_PN[port]->status == "out") return "You are already logged out\n";
    USERS_PN[port]->status = "out";
    return "Logged out\n";
}

string create_group(string group_id, string owner_port)
{
    if(USERS_PN.find(owner_port) == USERS_PN.end()) return "This account doesn't exist. Create a new account\n";
    Peer* owner = USERS_PN[owner_port];
    if(owner->status == "out") return "You need to login first\n";
    if(GROUPS.find(group_id) != GROUPS.end()) return "This group already exists. Pick another group id\n";
    Group* new_group = new Group(group_id, owner_port);
    new_group->add_member(owner);
    GROUPS.insert({group_id, new_group});
    return "New group created. You are now the owner of " + group_id + "\n";
}

string join_group(string group_id, string port)
{
    if(USERS_PN.find(port) == USERS_PN.end()) return "This account doesn't exist. Create a new account\n";
    Peer* requester = USERS_PN[port];
    if(requester->status == "out") return "You need to login first\n";
    if(GROUPS.find(group_id) == GROUPS.end()) return "This group doesn't exist\n";
    Group* g = GROUPS[group_id];
    if(g->members.find(requester) != g->members.end()) return "You are already a member of this group\n";
    if(g->pending_list.find(requester) != g->pending_list.end()) return "You have already requested tp join this group. Wait for the owner to accept your request\n";
    g->add_to_pending(port);
    return "Request sent. Wait for the owner to accept your request\n";
}

string leave_group(string group_id, string port)
{
    if(USERS_PN.find(port) == USERS_PN.end()) return "This account doesn't exist. Create a new account\n";
    Peer* deserter = USERS_PN[port];
    if(deserter->status == "out") return "You need to login first\n";
    if(GROUPS.find(group_id) == GROUPS.end()) return "This group doesn't exist\n";
    Group* g = GROUPS[group_id];
    if(g->pending_list.find(deserter) != g->pending_list.end())
    {
        g->pending_list.erase(deserter);
        return "Your request to join this group is revoked\n";
    }
    if(g->members.find(deserter) == g->members.end()) return "You are not in this group\n";
    g->members.erase(deserter);
    return "You are no longer a part of group " + group_id + "\n";
}

string list_pending(string group_id, string port)
{
    if(USERS_PN.find(port) == USERS_PN.end()) return "This account doesn't exist. Create a new account\n";
    Peer* requester = USERS_PN[port];
    if(requester->status == "out") return "You need to login first\n";
    if(GROUPS.find(group_id) == GROUPS.end()) return "This group doesn't exist\n";
    Group* g = GROUPS[group_id];
    if(g->owner != requester) return "You do not have permission to perform this action\n";
    string resp = "Following people have requested to join group " + group_id + "\n";
    unordered_set<Peer*>::iterator it;
    for(it = g->pending_list.begin(); it != g->pending_list.end(); it++)
    {
        resp += (*it)->username + "\n";
    }
    return resp;
}

string accept_request(string group_id, string user_id, string accepter_port)
{
    if(USERS_PN.find(accepter_port) == USERS_PN.end()) return "This account doesn't exist. Create a new account\n";   
    Peer* accepter = USERS_PN[accepter_port];
    if(accepter->status == "out") return "You need to login first\n";
    if(GROUPS.find(group_id) == GROUPS.end()) return "This group does not exist\n";
    if(USERS_NAME.find(user_id) == USERS_NAME.end()) return "This user does not exist\n";
    Peer* requester = USERS_NAME[user_id];
    Group* g = GROUPS[group_id];
    if(g->owner != accepter) return "You do not have permission to perform this action\n";
    if(g->members.find(requester) != g->members.end()) return "This user is already a member of this group\n";
    if(g->pending_list.find(requester) == g->pending_list.end()) return "Cannot add. This user has not requested to join the group\n";
    g->pending_list.erase(requester);
    g->add_member(requester);
    return "Member added to group\n";
}

string list_groups()
{
    string resp = "list of groups in this network:\n";
    unordered_map<string, Group*>::iterator it;
    for(it = GROUPS.begin(); it != GROUPS.end(); it++) resp += it->first + "\n";
    return resp;
}

string list_files(string group_id)
{
    if(GROUPS.find(group_id) == GROUPS.end()) return "This group doesn't exist\n";
    Group* g = GROUPS[group_id];
    string resp = "List of files in " + group_id + "\n";
    unordered_set<string>::iterator it;
    for(it = g->file_names.begin(); it != g->file_names.end(); it++) resp += *it + "\n";
    return resp;
}

string get_name_from_file_path(string filepath)
{
    int i = filepath.size() - 1;
    string name = "";
    while(i >= 0 && filepath[i] != '/') name.push_back(filepath[i--]);
    reverse(name.begin(), name.end());
    return name;
}

string upload_file(string filepath, string group_id, string port)
{
    if(USERS_PN.find(port) == USERS_PN.end()) return "This account doesn't exist. Create a new account\n";
    Peer* uploader = USERS_PN[port];
    if(uploader->status == "out") return "You need to login first\n";
    if(GROUPS.find(group_id) == GROUPS.end()) return "Group doesn't exist\n";
    Group* g = GROUPS[group_id];
    if(g->members.find(uploader) == g->members.end()) return "Cannot upload. You are not in this group\n";
    char buffer[PATH_MAX];
    char* abs_path = realpath(filepath.c_str(), buffer);
    if(!abs_path) return "This file doesn't exist\n";
    string file_name = get_name_from_file_path(string(abs_path));
    File* f = new File(file_name, string(abs_path), port);
    g->files.insert(f);
    g->file_names.insert(file_name);
    return "File uploaded\n";
}

File* get_file_from_name(Group* g, string name)
{
    for(File* f : g->files)
    {
        if(f->name == name) return f;
    }
    return NULL;
}

string send_file_details(string group_id, string file_name, string dest_port)
{
    if(GROUPS.find(group_id) == GROUPS.end()) return "This group doesn't exist\n";
    Peer* requester = USERS_PN[dest_port];
    Group* g = GROUPS[group_id];
    if(g->members.find(requester) == g->members.end()) return "Cannot download file. You are not a part of this group\n";
    if(g->file_names.find(file_name) == g->file_names.end()) return "This file doesn't exist in group\n";
    File* f = get_file_from_name(g, file_name);
    string resp = f->path + " " + f->owner_port;
    return resp;
}

string stop_sharing(string group_id, string file_name)
{
    if(GROUPS.find(group_id) == GROUPS.end()) return "This group doesn't exist\n";
    Group* g = GROUPS[group_id];
    if(g->file_names.find(file_name) == g->file_names.end()) return "This file doesn't exist\n";
    g->file_names.erase(file_name);
    return "Stopped uploading\n";
}

string execute_command(string client_msg)
{
    vector<string> command_tokens;
    tokenize(client_msg, command_tokens);
    string command = command_tokens[0];
    if(command == "create_user") return create_user(command_tokens[1], command_tokens[2], command_tokens[3]);
    else if (command == "send_message") return send_message(command_tokens[1]);
    else if (command == "login") return login(command_tokens[1], command_tokens[2]);
    else if (command == "logout") return logout(command_tokens[1]);
    else if (command == "create_group") return create_group(command_tokens[1], command_tokens[2]);
    else if (command == "join_group") return join_group(command_tokens[1], command_tokens[2]);
    else if (command == "leave_group") return leave_group(command_tokens[1], command_tokens[2]);
    else if (command == "requests") return list_pending(command_tokens[2], command_tokens[3]);
    else if (command == "accept_request") return accept_request(command_tokens[1], command_tokens[2], command_tokens[3]);
    else if (command == "list_groups") return list_groups();
    else if (command == "list_files") return list_files(command_tokens[1]);
    else if (command == "upload_file") return upload_file(command_tokens[1], command_tokens[2], command_tokens[3]);
    else if (command == "download_file") return send_file_details(command_tokens[1], command_tokens[2], command_tokens[4]);
    else if (command == "show_downloads") return " ";
    else if (command == "stop_share") return stop_sharing(command_tokens[1], command_tokens[2]);
    return "Invalid command";
}

void* handle_connection(void* args)
{
    int client_sock_fd = *(int*) args;
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);
    int bytes_transferred = read(client_sock_fd, buffer, BUFFER_SIZE - 1);
    if(bytes_transferred < 0) debug("Could not read from socket");
    cout<<"Command from client: "<<buffer<<"\n";
    string client_msg = buffer;
    string response = execute_command(client_msg);
    bytes_transferred = write(client_sock_fd, response.c_str(), response.size());
    if(bytes_transferred < 0) debug("Could not send message to client");
    return NULL;
}

/*reads the tracker_info.txt file (filepath has path to this file)
and populates tracker_addr struct with the tracker details*/
void get_tracker_details(string filepath, struct sockaddr_in* tracker_addr)
{
    bzero((char*) tracker_addr, sizeof(*tracker_addr));
    ifstream fin;
    fin.open(filepath);
    string line;
    getline(fin, line);
    inet_pton(AF_INET, line.c_str(), &(tracker_addr->sin_addr));
    getline(fin, line);
    tracker_addr->sin_port = htons(stoi(line));
    tracker_addr->sin_family = AF_INET;
    fin.close();
}

void* wait_for_quit(void* arg)
{
    while(true)
    {
        string cmd;
        cin>>cmd;
        if(cmd == "quit") exit(0);
    }
}

int main(int argc, char* argv[])
{
    int dummy = 0;
    pthread_t t;
    pthread_create(&t, NULL, wait_for_quit, &dummy);
    fout.open("tracker_log.txt", ios::out);
    struct sockaddr_in tracker_addr, client_addr;
    get_tracker_details(argv[1], &tracker_addr);
    //AF_INET: IPV4; SOCK_STREAM: TCP Socket, 0: OS will use the appropriate protocol
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0) debug("Could not open socket");
    int opt = 1;
	setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    if(bind(sock_fd, (struct sockaddr*) &tracker_addr, sizeof(tracker_addr)) < 0) debug("Could not bind socket");
    listen(sock_fd, BACKLOG);
    while(true)
    {
        socklen_t client_len = sizeof(client_addr);
        int client_sock_fd = accept(sock_fd, (struct sockaddr*) &client_addr, &client_len);
        //program will be blocked here until a request from a client is received
        if(client_sock_fd < 0) debug("Could not accept a new connection.");
        pthread_t t;
        pthread_create(&t, NULL, handle_connection, &client_sock_fd);
        //handle_connection(args);
    }
    fout.close();
    //close(client_sock_fd);
    close(sock_fd);
    return 0;
}