#include <cstdlib>
#include <chrono>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <memory>
#include <boost/thread.hpp>
#include <future>
#include <stdlib.h>

#include "cpp_h5_writer/RestApi.hpp"
#include "cpp_h5_writer/ProcessManager.hpp"
#include "cpp_h5_writer/config.hpp"
#include "cpp_h5_writer/H5Writer.hpp"

using namespace std;

namespace SfProcessManager {

    void notify_first_pulse_id(const string& bsread_rest_address, uint64_t pulse_id) {
        // First pulse_id should be an async operation - we do not want to make the writer wait.
        async(launch::async, [pulse_id, &bsread_rest_address]{
            cout << "Sending first received pulse_id " << pulse_id << " to bsread_rest_address " << bsread_rest_address << endl;

            stringstream request;
            request << "curl -X PUT " << bsread_rest_address << " ";
            request << "-H \"Content-Type: application/json\" ";
            request << "-d '{\"start_pulse_id\":" << pulse_id << "}'";

            string request_call(request.str());

            #ifdef DEBUG_OUTPUT
                cout << "[SfProcessManager::notify_first_pulse_id] Sending request (" << request_call << ")." << endl;
            #endif

            system(request_call.c_str());
        });
    }

    void notify_last_pulse_id(const string& bsread_rest_address, uint64_t pulse_id) {
        // Last pulse_id should be a sync operation - we do not want to terminate the process to quickly.
        cout << "Sending last received pulse_id " << pulse_id << " to bsread address " << bsread_rest_address << endl;

        stringstream request;
        request << "curl -X PUT " << bsread_rest_address << " ";
        request << "-H \"Content-Type: application/json\" ";
        request << "-d '{\"stop_pulse_id\":" << pulse_id << "}'";

        string request_call(request.str());

        #ifdef DEBUG_OUTPUT
            cout << "[SfProcessManager::notify_last_pulse_id] Sending request (" << request_call << ")." << endl;
        #endif

        system(request_call.c_str());
    }
    void receive_zmq(WriterManager& manager, RingBuffer& ring_buffer,
        ZmqReceiver& receiver, const H5Format& format)
    {

        receiver.connect();

        while (manager.is_running()) {
            
            auto frame = receiver.receive();
            
            // In case no message is available before the timeout, both pointers are NULL.
            if (!frame.first){
                continue;
            }

            auto frame_metadata = frame.first;
            auto frame_data = frame.second;

            #ifdef DEBUG_OUTPUT
                cout << "[SfProcessManager::receive_zmq] Processing FrameMetadata"; 
                cout << " with frame_index " << frame_metadata->frame_index;
                cout << " and frame_shape [" << frame_metadata->frame_shape[0] << ", " << frame_metadata->frame_shape[1] << "]";
                cout << " and endianness " << frame_metadata->endianness;
                cout << " and type " << frame_metadata->type;
                cout << " and frame_bytes_size " << frame_metadata->frame_bytes_size;
                cout << "." << endl;
            #endif

            // Commit the frame to the buffer.
            ring_buffer.write(frame_metadata, frame_data);

            manager.received_frame(frame_metadata->frame_index);
        }

        #ifdef DEBUG_OUTPUT
            cout << "[SfProcessManager::receive_zmq] Receiver thread stopped." << endl;
        #endif
    }

    void write_h5(WriterManager& manager, const H5Format& format, RingBuffer& ring_buffer,
        const shared_ptr<unordered_map<string, string>> header_values_type, const string& bsread_rest_address)
    {
        H5Writer writer(manager.get_output_file(), 0, config::initial_dataset_size, config::dataset_increase_step);
        auto raw_frames_dataset_name = config::raw_image_dataset_name;

        // Mapping for header values.
        // TODO: This should be moved into future PROTOCOL FORMAT file.
        std::unordered_map<std::string, int> type_to_size_mapping {
            {"uint8", 1},
            {"uint16", 2},
            {"uint32", 4},
            {"uint64", 8},
            {"int8", 1},
            {"int16", 2},
            {"int32", 4},
            {"int64", 8},
            {"float32", 4},
            {"float64", 8},
        };
        
        uint64_t last_pulse_id = 0;
        
        // Run until the running flag is set or the ring_buffer is empty.  
        while(manager.is_running() || !ring_buffer.is_empty()) {
            
            if (ring_buffer.is_empty()) {
                boost::this_thread::sleep_for(boost::chrono::milliseconds(config::ring_buffer_read_retry_interval));
                continue;
            }

            const pair< shared_ptr<FrameMetadata>, char* > received_data = ring_buffer.read();
            
            // NULL pointer means that the ringbuffer->read() timeouted. Faster than rising an exception.
            if(!received_data.first) {
                continue;
            }

            // Write image data.
            writer.write_data(raw_frames_dataset_name,
                            received_data.first->frame_index, 
                            received_data.second,
                            received_data.first->frame_shape,
                            received_data.first->frame_bytes_size, 
                            received_data.first->type,
                            received_data.first->endianness);

            ring_buffer.release(received_data.first->buffer_slot_index);

            // Write image metadata if mapping specified.
            if (header_values_type) {

                for (const auto& header_type : *header_values_type) {

                    auto& name = header_type.first;
                    auto& type = header_type.second;

                    auto value = received_data.first->header_values.at(name);

                    // Ugly hack until we get the start sequence in the bsread stream itself.
                    if (name == "pulse_id") {
                        if (!last_pulse_id) {
                            last_pulse_id = *(reinterpret_cast<uint64_t*>(value.get()));
                            notify_first_pulse_id(bsread_rest_address, last_pulse_id);
                        } else {
                            last_pulse_id = *(reinterpret_cast<uint64_t*>(value.get()));
                        }
                    }

                    // Header data are fixed to scalars in little endian.
                    vector<size_t> value_shape = {1};
                    auto endianness = "little";
                    auto value_bytes_size = type_to_size_mapping.at(type);

                    writer.write_data(name,
                                    received_data.first->frame_index,
                                    value.get(),
                                    value_shape,
                                    value_bytes_size,
                                    type,
                                    endianness);
                }
            }
            
            manager.written_frame(received_data.first->frame_index);
        }

        // Send the last_pulse_id only if it was set.
        if (last_pulse_id) {
            notify_last_pulse_id(bsread_rest_address, last_pulse_id);
        }

        if (writer.is_file_open()) {
            #ifdef DEBUG_OUTPUT
                cout << "[ProcessManager::write] Writing file format." << endl;
            #endif

            // Wait until all parameters are set or writer is killed.
            while (!manager.are_all_parameters_set() && !manager.is_killed()) {
                boost::this_thread::sleep_for(boost::chrono::milliseconds(config::parameters_read_retry_interval));
            }

            // Need to check again if we have all parameters to write down the format.
            if (manager.are_all_parameters_set()) {
                const auto parameters = manager.get_parameters();
                
                // Even if we can't write the format, lets try to preserve the data.
                try {
                    H5FormatUtils::write_format(writer.get_h5_file(), format, parameters);
                } catch (const runtime_error& ex) {
                    cerr << "[ProcessManager::write] Error while trying to write file format: "<< ex.what() << endl;
                }
            }
        }
        
        #ifdef DEBUG_OUTPUT
            cout << "[ProcessManager::write] Closing file " << manager.get_output_file() << endl;
        #endif
        
        writer.close_file();

        #ifdef DEBUG_OUTPUT
            cout << "[ProcessManager::write] Writer thread stopped." << endl;
        #endif

        // Exit when writer thread has closed the file.
        exit(0);
    }

    void run_writer(WriterManager& manager, const H5Format& format, ZmqReceiver& receiver, uint16_t rest_port,
        const std::string& bsread_rest_address)
    {
        size_t n_slots = config::ring_buffer_n_slots;
        RingBuffer ring_buffer(n_slots);

        #ifdef DEBUG_OUTPUT
            cout << "[SfProcessManager::run_writer] Running writer";
            cout << " and output_file " << manager.get_output_file();
            cout << " and n_slots " << n_slots;
            cout << endl;
        #endif

        boost::thread receiver_thread(receive_zmq, boost::ref(manager), boost::ref(ring_buffer), 
            boost::ref(receiver), boost::ref(format));
        boost::thread writer_thread(write_h5, boost::ref(manager), 
            boost::ref(format), boost::ref(ring_buffer), receiver.get_header_values_type(), 
            boost::ref(bsread_rest_address));

        RestApi::start_rest_api(manager, rest_port);

        #ifdef DEBUG_OUTPUT
            cout << "[SfProcessManager::run_writer] Rest API stopped." << endl;
        #endif

        // In case SIGINT stopped the rest_api.
        manager.stop();

        receiver_thread.join();
        writer_thread.join();

        #ifdef DEBUG_OUTPUT
            cout << "[SfProcessManager::run_writer] Writer properly stopped." << endl;
        #endif
    }
}