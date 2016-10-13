#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <signal.h>
#include <pcap.h>
#include "include.hpp"
#include "process_assembler.hpp"
#include "process_capture.hpp"
#include "process_flusher.hpp"
#include "process_worker.hpp"
#include "sip_header_processing.hpp"
#include <libpq-fe.h>


using boost::interprocess::create_only;
using boost::interprocess::offset_t;
using boost::interprocess::read_write;


std::string CONFIG_NAME                                = "SIP/RTP Sniffer";
std::string CONFIG_VERSION                             = "0.2";
std::string CONFIG_AUTHOR                              = "mx.dot";
uint16_t CONFIG_CAPTURE_DURATION                       = 20; // in seconds
std::string CONFIG_RTP_DESTINATION_DIRECTORY           = "/memory/media_output/"; // for data and metafiles
std::string CONFIG_SIP_DESTINATION_DIRECTORY           = "/memory/header_output/"; // for SIP headers
uint32_t CONFIG_RTP_DESTINATION_DIRECTORY_MAXSIZE      = 1024; // in mb
uint32_t CONFIG_SIP_DESTINATION_DIRECTORY_MAXSIZE      = 512; // in mb
uint32_t CONFIG_RTP_DESTINATION_DIRECTORY_MAX_FILE_CNT = 10240; // maximum files in directory
uint32_t CONFIG_SIP_DESTINATION_DIRECTORY_MAX_FILE_CNT = 102400; // maximum files in directory
uint16_t CONFIG_WORKING_PROCESSES                      = 10; // number of workers
uint16_t CONFIG_STREAM_TIMEOUT                         = 30; // in seconds
uint16_t CONFIG_SIP_HEADER_TIMEOUT                     = 1; // in minutes
uint16_t CONFIG_DISPATCHER_TIMEOUT                     = 5; // in seconds
uint32_t CONFIG_PCAP_BUFFER_SIZE                       = 10240000;
bool CONFIG_MODE_DUMP_SIP_HEADERS                      = true;
bool CONFIG_MODE_DUMP_RTP_STREAMS                      = true;
std::string CONFIG_CAPTURE_OPTIONS                     = "udp";
std::string CONFIG_NETWORK_INTERFACE                   = "eth0";
uint16_t CONFIG_SHM_COUNT                              = CONFIG_WORKING_PROCESSES + 2; // 1 per process and 1 for capturing and 1 for controller


std::string CONFIG_VAD_IP           = "192.168.112.86";
std::string CONFIG_VAD_DB           = "class4_v5";
std::string CONFIG_VAD_USER         = "class4_dial";
std::string CONFIG_VAD_PWD          = "";
std::string CONFIG_VAD_TABLE        = "public.class4_dial";


bool handle_exit = false;
std::set<Codec> allowed_codecs;
std::map<Codec, struct codec_details> codec_lookup_table;
std::deque<uint16_t> process_pid_storage;
pcap_t* pcap_handle = nullptr;


void init_codecs()
{
    // configuring
    allowed_codecs.insert(Codec::g711U);
    allowed_codecs.insert(Codec::g711A);
    allowed_codecs.insert(Codec::g723);
    allowed_codecs.insert(Codec::g729);

    codec_lookup_table[Codec::g711U] = {"g711U", 50};
    codec_lookup_table[Codec::g711A] = {"g711A", 50};
    codec_lookup_table[Codec::g723] = {"g723", 50};
    codec_lookup_table[Codec::g729] = {"g729", 50};
}

//----------------------------------------------------------------------

void init_shared_memory()
{
    static std::deque<std::unique_ptr<struct shm_remove>> removers;

    for (uint16_t i = 0; i < CONFIG_SHM_COUNT; ++i) {
        std::stringstream out;
        out << "shm" << i;

        auto name = out.str();
        auto remover = std::make_unique<struct shm_remove>(name);
        removers.push_back(std::move(remover));


        shared_memory_object shm(create_only, name.c_str(), read_write);
        shm.truncate(sizeof(shared_memory_segment));

        offset_t shm_size = 0;
        if (!shm.get_size(shm_size)) {
            std::cerr << "[err] Unable get size of shared memory segment. Exiting..." << std::endl;
            exit(EXIT_FAILURE);
        }

        if (shm_size != sizeof(shared_memory_segment)) {
            std::cerr << "[err] Unable allocate shared memory segment. Unable to process. Exiting..." << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

//----------------------------------------------------------------------

void show_usage(std::string name)
{
    std::cout << "Usage: " << name << " <option(s)>\n"
              << "Options:\n"
              << "\t-h, --help (Show this help message)\n"
              << "\t--rtp_dir PATH (Path to save RTP media output)\n"
              << "\t--sip_dir PATH (Path to save SIP headrs)\n"
              << "\t--rtp_size SIZE (Max size to store RTP media output date in megabytes)\n"
              << "\t--sip_size SIZE (Max size to store SIP headers output date in megabytes)\n"
              << "\t--rtp_cnt COUNT (Max count of files in RTP directory)\n"
              << "\t--sip_cnt COUNT (Max count of files in RTP directory)\n"
              << "\t-d, --duration TIME (Specify the duration to grab into seconds)\n"
              << "\t-o, --options LIBPCAP CAPTURE OPTIONS (Specify options format in libpcap format)\n"
              << "\t-i, --interface NETWORK INTERFACE NAME (Specify the device name)\n"
              << "\t-w, --workers WORKERS NUMBER (Specify processes quantity)\n"
              << "\t-c, --cache CACHED PACKETS NUMBER (Specify cache size for network packets)\n"
              << "\t-m, --mode MODE (Specify mode: rtp (default) for dump media streams, sip for SIP header and all for both)\n"
              << "\t --vadip, VAD remote host ip address\n"
              << "\t --vaddb, VAD remote postgress database name\n"
              << "\t --vaduser, VAD remote postgres user\n"
              << "\t --vadpwd, VAD remote postgres password\n"
              << "\t --vadtable, VAD remote postgres table\n"
              << std::endl;
}

//----------------------------------------------------------------------

void parse_arguments(int argc, char** argv)
{
    for (int16_t i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if ((arg == "-h") || (arg == "--help")) {
            show_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
        
        else if (arg == "--vadip") { // RTP dir argument
            if (i + 1 < argc) {
                CONFIG_VAD_IP = argv[++i];
            }
        }
        else if (arg == "--vaddb") { // RTP dir argument
            if (i + 1 < argc) {
                CONFIG_VAD_DB = argv[++i];
            }
        }
        else if (arg == "--vaduser") { // RTP dir argument
            if (i + 1 < argc) {
                CONFIG_VAD_USER = argv[++i];
            }
        }
        
        else if (arg == "--vadpwd") { // RTP dir argument
            if (i + 1 < argc) {
                CONFIG_VAD_PWD = argv[++i];
            }
        }
        
        else if (arg == "--vadtable") { // RTP dir argument
            if (i + 1 < argc) {
                CONFIG_VAD_TABLE = argv[++i];
            }
        }
        
        
        else if (arg == "--rtp_dir") { // RTP dir argument
            if (i + 1 < argc) {
                CONFIG_RTP_DESTINATION_DIRECTORY = argv[++i];
            }
        }
        else if (arg == "--sip_dir") { // SIP dir argument
            if (i + 1 < argc) {
                CONFIG_SIP_DESTINATION_DIRECTORY = argv[++i];
            }
        }
        else if (arg == "--rtp_size") { // RTP dir max size argument
            if (i + 1 < argc) {
                CONFIG_RTP_DESTINATION_DIRECTORY_MAXSIZE = atoi(argv[++i]);
            }
        }
        else if (arg == "--sip_size") { // SIP dir max size argument
            if (i + 1 < argc) {
                CONFIG_SIP_DESTINATION_DIRECTORY_MAXSIZE = atoi(argv[++i]);
            }
        }
        else if (arg == "--rtp_cnt") { // RTP dir max count argument
            if (i + 1 < argc) {
                CONFIG_RTP_DESTINATION_DIRECTORY_MAX_FILE_CNT = atoi(argv[++i]);
            }
        }
        else if (arg == "--sip_cnt") { // SIP dir max count argument
            if (i + 1 < argc) {
                CONFIG_SIP_DESTINATION_DIRECTORY_MAX_FILE_CNT = atoi(argv[++i]);
            }
        }
        else if ((arg == "-d") || (arg == "--duration")) { // duration argument
            if (i + 1 < argc) {
                CONFIG_CAPTURE_DURATION = atoi(argv[++i]);
            }
        }
        else if ((arg == "-o") || (arg == "--options")) { // range argument
            if (i + 1 < argc) {
                CONFIG_CAPTURE_OPTIONS = argv[++i];
            }
        } 
        else if ((arg == "-i") || (arg == "--interface")) { // interface argument
            if (i + 1 < argc) {
                CONFIG_NETWORK_INTERFACE = argv[++i];
            }
        }
        else if ((arg == "-w") || (arg == "--workers")) { // processes argument
            if (i + 1 < argc) {
                CONFIG_WORKING_PROCESSES = atoi(argv[++i]);
            }
        }
        else if ((arg == "-m") || (arg == "--mode")) { // mode argument
            if (i + 1 < argc) {
                std::string mode = argv[++i];

                if (mode == "sip") {
                    CONFIG_MODE_DUMP_SIP_HEADERS = true;
                    CONFIG_MODE_DUMP_RTP_STREAMS = false;
                    CONFIG_CAPTURE_OPTIONS = "udp port 5060";
                }
                if (mode == "rtp") {
                    CONFIG_MODE_DUMP_SIP_HEADERS = false;
                    CONFIG_MODE_DUMP_RTP_STREAMS = true;
                }
                if (mode == "all") {
                    CONFIG_MODE_DUMP_SIP_HEADERS = true;
                    CONFIG_MODE_DUMP_RTP_STREAMS = true;
                }
            }
        }
    }

    CONFIG_SHM_COUNT = CONFIG_WORKING_PROCESSES + 2;
}

//----------------------------------------------------------------------

void show_config()
{
    std::cout << std::endl;
    std::cout << ">>> " << CONFIG_NAME << std::endl;
    std::cout << ">>> version " << CONFIG_VERSION << std::endl;
    std::cout << ">>> author  " << CONFIG_AUTHOR << std::endl;
    std::cout << std::endl;

    print_line();
    std::cout << "RTP working directory                 - " << CONFIG_RTP_DESTINATION_DIRECTORY << std::endl;
    std::cout << "SIP working directory                 - " << CONFIG_SIP_DESTINATION_DIRECTORY << std::endl;
    std::cout << "max RTP working directory size        - " << CONFIG_RTP_DESTINATION_DIRECTORY_MAXSIZE << " mb" << std::endl;
    std::cout << "max SIP working directory size        - " << CONFIG_SIP_DESTINATION_DIRECTORY_MAXSIZE << " mb" << std::endl;
    std::cout << "max RTP working directory files count - " << CONFIG_RTP_DESTINATION_DIRECTORY_MAX_FILE_CNT << std::endl;
    std::cout << "max SIP working directory files count - " << CONFIG_SIP_DESTINATION_DIRECTORY_MAX_FILE_CNT << std::endl;
    std::cout << "record duration to capture            - " << CONFIG_CAPTURE_DURATION << " seconds" << std::endl;
    std::cout << "number of workers                     - " << CONFIG_WORKING_PROCESSES << std::endl;
    std::cout << "network interface name                - " << CONFIG_NETWORK_INTERFACE << std::endl;
    std::cout << "pcap capture options                  - " << CONFIG_CAPTURE_OPTIONS << std::endl;
    std::cout << "VAD IP                                - " << CONFIG_VAD_IP << std::endl;
    std::cout << "VAD DB                                - " << CONFIG_VAD_DB << std::endl;
    std::cout << "VAD USER                              - " << CONFIG_VAD_USER << std::endl;
    std::cout << "VAD PWD                               - " << CONFIG_VAD_PWD << std::endl;
    std::cout << "VAD TABLE                             - " << CONFIG_VAD_TABLE << std::endl;

    std::string mode = "";
    if (CONFIG_MODE_DUMP_RTP_STREAMS) {
        mode += "rtp ";
    }
    if (CONFIG_MODE_DUMP_SIP_HEADERS) {
        mode += "sip";
    }
    std::cout << "capture mode                          - " << mode << std::endl;
    print_line();
    std::cout << std::endl;
}

//----------------------------------------------------------------------

void signal_handler(int)
{
    handle_exit = true;
}

//----------------------------------------------------------------------

template<typename Func>
void launch_process(Func func)
{
    uint16_t pid = fork();
    if (!pid) {
        process_pid_storage.push_back(pid);
    } else {
        func();
    }
}

static PGconn* postgress_connect()
{
	PGconn *conn = NULL;
	char conninfo_str[256];
    char sSql[256];
    
	//sprintf(conninfo_str, "user='postgres' password='' dbname='class4_v5' hostaddr='127.0.0.1'");

	sprintf(conninfo_str, "user='%s' password='%s' dbname='%s' hostaddr='%s'",
	    CONFIG_VAD_USER.c_str(), CONFIG_VAD_PWD.c_str(), CONFIG_VAD_DB.c_str(), CONFIG_VAD_IP.c_str());
	
	conn = PQconnectdb(conninfo_str);
	// Check to see that the back-end connection was successfully made
	if (PQstatus(conn) != CONNECTION_OK)
	{
	    
		printf("Connection to database failed\n");
		std::cout << "VAD IP                                - " << CONFIG_VAD_IP << std::endl;
        std::cout << "VAD DB                                - " << CONFIG_VAD_DB << std::endl;
        std::cout << "VAD USER                              - " << CONFIG_VAD_USER << std::endl;
        std::cout << "VAD PWD                               - " << CONFIG_VAD_PWD << std::endl;
        std::cout << "VAD TABLE                             - " << CONFIG_VAD_TABLE << std::endl;
		PQfinish(conn);
        exit_nicely();
		return NULL;
	}
    

	PGresult *res = NULL;
	//sprintf(sSql, "CREATE TABLE IF NOT EXISTS public.fas_check_result (ID serial PRIMARY KEY, callid VARCHAR (255) NOT NULL, calltime VARCHAR (255) NOT NULL, result smallint);");
	sprintf(sSql, "CREATE TABLE IF NOT EXISTS %s (ID serial PRIMARY KEY, callid VARCHAR (255) NOT NULL, calltime VARCHAR (255) NOT NULL, result smallint);", CONFIG_VAD_TABLE.c_str());
	res = PQexec(conn, sSql);

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("Create table failed\n");
		PQclear(res);
		PQfinish(conn);
		return NULL;
	}	
	// Clear result
	PQclear(res);
	return conn;
}

//----------------------------------------------------------------------
int InsertRecord(PGconn *conn, const char* table_name, const char* calltime, const char *callid,int result)
{
	// Append the SQL statement
	char sSql[256];
	sprintf(sSql, "INSERT INTO %s(callid,calltime,result) VALUES('%s', '%s', '%d')", table_name, callid,calltime,result);

	// Execute with SQL statement
	PGresult *res = PQexec(conn, sSql);
    ExecStatusType  status = PQresultStatus(res);
    printf("Postgres Result Status = %s\n",PQresStatus(status));
        
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("Insert record failed\n");
		printf("Postgres error string=%s\n",PQresultErrorMessage(res));
		PQclear(res);
		PQfinish(conn);
		return -1;
	}

	// Clear result
	PQclear(res);
	return 0;
}

int main(int argc, char** argv)
{
    parse_arguments(argc, argv);
    
    show_config();
    
    printf("postgress database connecting\n");
    PGconn* conn= postgress_connect();
    if(conn != NULL)
    {
        printf("postgress database connection success\n");
        /*int i;
        char timebuf[64];
        char callid[64];
        for(i = 0; i < 10; i++)
        {
            time_t t = time(NULL);
            struct tm tm = *localtime(&t); 
            
            sprintf(timebuf,"%d-%d-%d %d:%d:%d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
            sprintf(callid,"call-%d-%d",tm.tm_min,i+1);
            InsertRecord(conn,CONFIG_VAD_TABLE.c_str(),timebuf,callid,i%2);
        }*/
        
        PQfinish(conn);
        // printf("wrote ten rows to database for test\n");
        // printf("sleeping 1 second....\n");
        // usleep(1000*1000);
        
    }else {
        printf("postgress database connection fail\n");
        exit_nicely();
    }
        
    // some additional initiations
    init_codecs();
    init_regex_patterns();

    // initialize shared memory storage
    init_shared_memory();


    // launch worker processes
    for (uint16_t i = 0; i < CONFIG_WORKING_PROCESSES; ++i) {
        launch_process(process_worker);
    }
    // launch capture process
    launch_process(process_capture);
    // launch assemble process
    launch_process(process_assembler);
    // launch flush process
    launch_process(process_flusher);


    // waiting for SIGINT to perminate processes
    signal(SIGINT, signal_handler);
    while (!handle_exit) {
        pause();
    }

    std::cout << "exiting..." << std::endl;

    for (auto pid: process_pid_storage) {
        kill(pid, SIGINT);
    }


    return EXIT_SUCCESS;
}
