#include <iostream>
#include <sstream>
#include <stdexcept>

#include "cpp_h5_writer/config.hpp"
#include "cpp_h5_writer/WriterManager.hpp"
#include "cpp_h5_writer/ZmqReceiver.hpp"

#include "SfFormat.cpp"
#include "SfProcessManager.hpp"

int main (int argc, char *argv[])
{
    if (argc != 7) {
        cout << endl;
        cout << "Usage: sf_cpp_h5_writer [connection_address] [output_file] [n_frames]";
        cout << " [rest_port] [user_id] [bsread_address]" << endl;
        cout << "\tconnection_address: Address to connect to the stream (PULL). Example: tcp://127.0.0.1:40000" << endl;
        cout << "\toutput_file: Name of the output file." << endl;
        cout << "\tn_frames: Number of images to acquire. 0 for infinity (until /stop is called)." << endl;
        cout << "\trest_port: Port to start the REST Api on." << endl;
        cout << "\tuser_id: uid under which to run the writer. -1 to leave it as it is." << endl;
        cout << "\tbsread_address: HTTP address of the bsread REST api." << endl;
        cout << endl;

        exit(-1);
    }

    // This process can be set to run under a different user.
    auto user_id = atoi(argv[5]);
    if (user_id != -1) {

        #ifdef DEBUG_OUTPUT
            cout << "[sf_cpp_h5_writer::main] Setting process uid to " << user_id << endl;
        #endif

        if (setgid(user_id)) {
            stringstream error_message;
            error_message << "[sf_cpp_h5_writer::main] Cannot set group_id to " << user_id << endl;

            throw runtime_error(error_message.str());
        }

        if (setuid(user_id)) {
            stringstream error_message;
            error_message << "[sf_cpp_h5_writer::main] Cannot set user_id to " << user_id << endl;

            throw runtime_error(error_message.str());
        }
    }

    int n_frames =  atoi(argv[3]);
    string output_file = string(argv[2]);
    string bsread_rest_address = string(argv[6]);

    // Create the output folder if it does not exist.
    size_t file_separator_index = output_file.rfind('/');
    
    // Do not create folders for a reletive filename.
    if (file_separator_index != string::npos) {
        string output_folder(output_file.substr(0, file_separator_index));

        cout << "[sf_cpp_h5_writer::main] Creating folder " << output_folder << endl;

        string create_folder_command("mkdir -p " + output_folder);

        cout << "[sf_cpp_h5_writer::main] Executing command system(" << create_folder_command << ");" << endl;
        system(create_folder_command.c_str());

    } else {
        cout << "[sf_cpp_h5_writer::main] Output filename '"<< output_file << "' appears to be relative. No folders will be created." << endl;
    }

    SfFormat format;
    
    WriterManager manager(format.get_input_value_type(), output_file, n_frames);

    string connect_address = string(argv[1]);
    int n_io_threads = config::zmq_n_io_threads;
    int receive_timeout = config::zmq_receive_timeout;
    auto header_values = shared_ptr<unordered_map<string, string>>(new unordered_map<string, string> {
        {"pulse_id", "uint64"},
        {"frame", "uint64"},
        {"is_good_frame", "uint64"},
        {"daq_rec", "int64"},

        {"missing_packets_1", "JF2.0M_header"},
        {"missing_packets_2", "JF2.0M_header"},
        {"daq_recs", "JF2.0M_header"},
        {"framenum_diff", "JF2.0M_header"},
        {"pulse_ids", "JF2.0M_header"},
        {"framenums", "JF2.0M_header"},
        {"pulse_id_diff", "JF2.0M_header"},
        {"module_number", "JF2.0M_header"},
    });
    ZmqReceiver receiver(connect_address, n_io_threads, receive_timeout, header_values);

    int rest_port = atoi(argv[4]);

    SfProcessManager::run_writer(manager, format, receiver, rest_port, bsread_rest_address);

    return 0;
}
