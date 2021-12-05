#include<iostream>
#include<strings.h> //bzero()
#include<sys/types.h>
#include<sys/socket.h> //socket(), bind(), listen(), accept()
#include<netinet/in.h> //struct sockaddr_in
#include<unistd.h> //read(), write()
#include<netdb.h>
#include<string.h> //strlen()
#include<pthread.h>
#include<vector>
#include<sstream>
#include<fstream>
#include<algorithm> //reverse()

using namespace std;

const int BUFFER_SIZE = 256;
const int BACKLOG = 10;
const int CHUNK_SIZE = 524288;
ofstream fout;
string TRACKER_FILE_PATH;
vector<pair<string, string> > DOWNLOADS;

struct handle_connection_args
{
    int client_sock_fd;
    struct sockaddr_in client_addr;
};

inline void debug(string msg)
{
    fout<<msg<<endl;
}

/*reads the tracker_info.txt file (filepath has path to this file)
and populates tracker_addr vector with the tracker details
tracker_addr[0] = ip address; tracker_addr[1] = port no */
inline void get_tracker_details(string filepath, vector<string>& tracker_addr)
{
    ifstream fin;
    fin.open(filepath);
    string line;
    getline(fin, line);
    tracker_addr.push_back(line);
    getline(fin, line);
    tracker_addr.push_back(line);
    fin.close();
}

void tokenize(string msg, vector<string>& tokens)
{
    tokens.clear();
    stringstream ss(msg);
    string token;
    while(ss>>token) tokens.push_back(token);
}

void send_message(string dest_port, string msg)
{
    in_port_t dest_port_no = htons(stoi(dest_port));
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0) debug("Cannot open socket");
    vector<string> tracker_addr;
    get_tracker_details(TRACKER_FILE_PATH, tracker_addr);
    struct hostent* server = gethostbyname(tracker_addr[0].c_str());
    if(!server) debug("No such host"); //server could not be located
    struct sockaddr_in server_addr;
    bzero((char*) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = dest_port_no;
    //sets the server address
    bcopy((char*) server->h_addr, (char*) &server_addr.sin_addr.s_addr, server->h_length);
    if(connect(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) debug("Cannot connect to server");
    int bytes_transferred = write(sock_fd, msg.c_str(), msg.size());
    if(bytes_transferred < 0) debug("Cannot write to socket");
    else debug("sent message: " + msg);
    close(sock_fd);
    debug("In send message: done");
}

string get_name_from_file_path(string filepath)
{
    int i = filepath.size() - 1;
    string name = "";
    while(i >= 0 && filepath[i] != '/') name.push_back(filepath[i--]);
    reverse(name.begin(), name.end());
    return name;
}

//file will be written to sock_fd
void send_file(int client_sock_fd, string abs_path)
{
    FILE* file_to_send = fopen(abs_path.c_str(), "r");
    char buffer[CHUNK_SIZE];
    int bytes_read = 0;
    int chunk_num = 0;
    while((bytes_read = fread(buffer, 1, CHUNK_SIZE, file_to_send)) > 0)
    {
        int bytes_sent = send(client_sock_fd, buffer, bytes_read, 0);
        if(bytes_sent < 0) debug("Failed to send chunk " + to_string(chunk_num));
        else{
            ++chunk_num;
            debug("Sent chunk " + to_string(chunk_num));
        }
        memset(buffer, 0, CHUNK_SIZE);
    }
    debug("sending done");
    fclose(file_to_send);
}

void* handle_connection(void* args)
{
    int client_sock_fd = *(int*) args;
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);
    int bytes_transferred = read(client_sock_fd, buffer, BUFFER_SIZE - 1);
    if(bytes_transferred < 0) debug("Could not read from socket");
    string cli_msg = buffer;
    vector<string> cli_msg_tokens;
    tokenize(cli_msg, cli_msg_tokens);
    if(cli_msg_tokens[0] == "send_file") send_file(client_sock_fd, cli_msg_tokens[1]);
    //else cout<<"Command from client: "<<buffer<<"\n";
    close(client_sock_fd);
    return NULL;
}

void* start_server(void* p_no)
{
    int port_no = *(int*) p_no;
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0) debug("Cannot open socket");
    //cout<<port_no<<" is listening: \n";
    int opt = 1;
	setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    struct sockaddr_in server_addr, client_addr;
    //initialize server_addr to 0
    bzero((char*) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    //convert port_no which is in host byte order to network byte order
    server_addr.sin_port = htons(port_no);
    server_addr.sin_addr.s_addr = INADDR_ANY; //s_addr now contains IP address of this machine
    if(bind(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) debug("Could not bind socket");
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
    //close(client_sock_fd);
    close(sock_fd);

}

//connects with tracker and sends command. Returns the response
string send_command_to_tracker(string command, string filepath)
{
    vector<string> tracker_addr;
    get_tracker_details(filepath, tracker_addr);
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0) debug("Cannot open socket");
    struct hostent* server = gethostbyname(tracker_addr[0].c_str());
    if(!server) debug("No such host"); //server could not be located
    struct sockaddr_in server_addr;
    bzero((char*) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    //sets the server address
    bcopy((char*) server->h_addr, (char*) &server_addr.sin_addr.s_addr, server->h_length);
    server_addr.sin_port = htons(stoi(tracker_addr[1]));
    if(connect(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) debug("Cannot connect to server");
    int bytes_transferred = write(sock_fd, command.c_str(), command.size());
    if(bytes_transferred < 0) debug("Cannot write to socket");
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);
    bytes_transferred = read(sock_fd, buffer, BUFFER_SIZE);
    if(bytes_transferred < 0) debug("Cannot read from socket");
    string response = buffer;
    cout<<"From tracker: "<<response;
    close(sock_fd);
    return response;
}



void recv_file_helper(int sock_fd, string dest_path, string file_name)
{
    dest_path += '/' + file_name;
    FILE* f = fopen(dest_path.c_str(), "w");
    char buffer[CHUNK_SIZE];
    int bytes_recvd = 0;
    int chunk_num = 0;
    while((bytes_recvd = recv(sock_fd, buffer, CHUNK_SIZE, 0)) > 0)
    {
        int bytes_written = fwrite(buffer, 1, bytes_recvd, f);
        if(bytes_written < 0) debug("Error writing chunk");
        else
        {
            ++chunk_num;
            debug("Written chunk " + to_string(chunk_num));
        }
    }
    debug("Done writing");
    fclose(f);
}

void recv_file(string sender_port, string sender_file_abs_path, string dest_path)
{
    in_port_t sender_port_no = htons(stoi(sender_port));
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0) debug("Cannot open socket");
    vector<string> tracker_addr;
    get_tracker_details(TRACKER_FILE_PATH, tracker_addr);
    struct hostent* server = gethostbyname(tracker_addr[0].c_str());
    if(!server) debug("No such host"); //server could not be located
    struct sockaddr_in server_addr;
    bzero((char*) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = sender_port_no;
    //sets the server address
    bcopy((char*) server->h_addr, (char*) &server_addr.sin_addr.s_addr, server->h_length);
    if(connect(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) debug("Cannot connect to server");
    else
    {
        string file_dwnld_msg = "send_file " + sender_file_abs_path;
        int bytes_transferred = write(sock_fd, file_dwnld_msg.c_str(), file_dwnld_msg.size());
        if(bytes_transferred < 0) debug("Cannot write to socket");
        else
        {
            debug("sent message: " + file_dwnld_msg);
            string file_name = get_name_from_file_path(sender_file_abs_path);
            recv_file_helper(sock_fd, dest_path, file_name);
        }
        close(sock_fd);
        debug("In recv_file: socket closed");
    }
}

//socket_addr is the socket address(ip:port). This function extracts and returns the port no
int get_port_no(string socket_addr)
{
    int i = 0;
    while(socket_addr[i] != ':') ++i;
    ++i;
    string port = "";
    while(i < socket_addr.size()) port.push_back(socket_addr[i++]);
    return stoi(port);
}

int main(int argc, char* argv[])
{   
    TRACKER_FILE_PATH = argv[2];
    //hardcode argv[3] before submitting
    fout.open("client_log.txt", ios::out);
    pthread_t t;
    int p_no = get_port_no(argv[1]);
    pthread_create(&t, NULL, start_server, &p_no);
    while(true)
    {
        cout<<"Enter command: ";
        string command;
        getline(cin, command);
        command = command + " " + to_string(p_no);
        vector<string> command_tokens;
        tokenize(command, command_tokens);
        string response = send_command_to_tracker(command, argv[2]);
        if(command_tokens[0] == "send_message")
        {
            string dest_port = response;
            send_message(dest_port, command_tokens[2]);
            debug("message sending done");
        }
        else if (command_tokens[0] == "download_file")
        {
            vector<string> file_details;
            tokenize(response, file_details);
            /*file_details[0] = path of file on server's machine; file_details[1] = port no of server;
            command_tokens[3] = destination_path for download*/
            string download_file_msg = "send_file " + file_details[0];
            recv_file(file_details[1], file_details[0], command_tokens[3]);
            DOWNLOADS.push_back({command_tokens[1], get_name_from_file_path(file_details[0])});
        }
        else if (command_tokens[0] == "show_downloads")
        {
            for(pair<string, string> p : DOWNLOADS)
            {
                cout<<"[C] "<<p.first<<" "<<p.second<<"\n";
            }
        }
    }
    fout.close();
    return 0;
}