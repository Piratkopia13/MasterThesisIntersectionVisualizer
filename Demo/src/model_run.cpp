#pragma once

//
// File originially from https://github.com/aljabr0/from-keras-to-c
// It has been slightly modified to fit the purposes of this program
//

#include "tensorflow/c/c_api.h"
#include <memory>
#include <iostream>
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <vector>
#include <assert.h>
#include <string.h>
#include <fstream>
#include <stdint.h>

static TF_Buffer *read_tf_buffer_from_file(const char* file);

/**
 * A Wrapper for the C API status object.
 */
class CStatus{
public:
    TF_Status *ptr;
    CStatus(){
        ptr = TF_NewStatus();
    }

    /**
     * Dump the current error message.
     */
    void dump_error()const{
        std::cerr << "TF status error: " << TF_Message(ptr) << std::endl;
    }

    /**
     * Return a boolean indicating whether there was a failure condition.
     * @return
     */
    inline bool failure()const{
        return TF_GetCode(ptr) != TF_OK;
    }

    ~CStatus(){
        if(ptr)TF_DeleteStatus(ptr);
    }
};

namespace detail {
    template<class T>
    class TFObjDeallocator;

    template<>
    struct TFObjDeallocator<TF_Status> { static void run(TF_Status *obj) { TF_DeleteStatus(obj); }};

    template<>
    struct TFObjDeallocator<TF_Graph> { static void run(TF_Graph *obj) { TF_DeleteGraph(obj); }};

    template<>
    struct TFObjDeallocator<TF_Tensor> { static void run(TF_Tensor *obj) { TF_DeleteTensor(obj); }};

    template<>
    struct TFObjDeallocator<TF_SessionOptions> { static void run(TF_SessionOptions *obj) { TF_DeleteSessionOptions(obj); }};

    template<>
    struct TFObjDeallocator<TF_Buffer> { static void run(TF_Buffer *obj) { TF_DeleteBuffer(obj); }};

    template<>
    struct TFObjDeallocator<TF_ImportGraphDefOptions> {
        static void run(TF_ImportGraphDefOptions *obj) { TF_DeleteImportGraphDefOptions(obj); }
    };

    template<>
    struct TFObjDeallocator<TF_Session> {
        static void run(TF_Session *obj) {
            CStatus status;
            TF_DeleteSession(obj, status.ptr);
            if (status.failure()) {
                status.dump_error();
            }
        }
    };
}

template<class T> struct TFObjDeleter{
    void operator()(T* ptr) const{
        detail::TFObjDeallocator<T>::run(ptr);
    }
};

template<class T> struct TFObjMeta{
    typedef std::unique_ptr<T, TFObjDeleter<T>> UniquePtr;
};

template<class T> typename TFObjMeta<T>::UniquePtr tf_obj_unique_ptr(T *obj){
    typename TFObjMeta<T>::UniquePtr ptr(obj);
    return ptr;
}

class MySession{
public:
    typename TFObjMeta<TF_Graph>::UniquePtr graph;
    typename TFObjMeta<TF_Session>::UniquePtr session;

    TF_Output inputs, outputs;
};



std::string GPUDeviceName(TF_Session* session) {
    std::unique_ptr<TF_Status, decltype(&TF_DeleteStatus)> status(
        TF_NewStatus(), TF_DeleteStatus);
    TF_Status* s = status.get();
    std::unique_ptr<TF_DeviceList, decltype(&TF_DeleteDeviceList)> list(
        TF_SessionListDevices(session, s), TF_DeleteDeviceList);
    TF_DeviceList* device_list = list.get();
    if (TF_OK != TF_GetCode(s))
        std::cout << TF_Message(s) << '\n';
    const int num_devices = TF_DeviceListCount(device_list);
    std::cout << "There are " << num_devices << " devices.";
    for (int i = 0; i < num_devices; ++i) {
        const char* device_name = TF_DeviceListName(device_list, i, s);
        if (TF_OK != TF_GetCode(s))
            std::cout << TF_Message(s) << '\n';
        const char* device_type = TF_DeviceListType(device_list, i, s);
        if (TF_OK != TF_GetCode(s))
            std::cout << TF_Message(s) << '\n';
        std::cout << "Device " << i << " has name " << device_name << ", type "
            << device_type;
        /*if (std::string(device_type) == DEVICE_GPU) {
            return device_name;
        }*/
    }
    // No GPU device found.
    return "";
}


/**
 * Load a GraphDef from a provided file.
 * @param filename: The file containing the protobuf encoded GraphDef
 * @param input_name: The name of the input placeholder
 * @param output_name: The name of the output tensor
 * @return
 */
MySession *my_model_load(const char *filename, const char *input_name, const char *output_name) {
    printf("Loading model %s\n", filename);
    CStatus status;

    auto graph=tf_obj_unique_ptr(TF_NewGraph());
    {
        // Load a protobuf containing a GraphDef
        auto graph_def=tf_obj_unique_ptr(read_tf_buffer_from_file(filename));
        if(!graph_def){
            return nullptr;
        }

        auto graph_opts=tf_obj_unique_ptr(TF_NewImportGraphDefOptions());
        TF_GraphImportGraphDef(graph.get(), graph_def.get(), graph_opts.get(), status.ptr);
    }

    if(status.failure()){
        status.dump_error();
        return nullptr;
    }

    auto input_op = TF_GraphOperationByName(graph.get(), input_name);
    auto output_op = TF_GraphOperationByName(graph.get(), output_name);
    if(!input_op || !output_op){
        return nullptr;
    }

    auto session = std::make_unique<MySession>();
    {
        TF_SessionOptions* sessionOpts = TF_NewSessionOptions();
        
        //const std::string configText = "introa_op_parallelism_threads: 4";
        //std::string configText = "device_count: { 'CPU' : 1, 'GPU' : 0 }";
            /*allow_soft_placement = True, \
            log_device_placement = False \
            ";*/

        //uint8_t config[11] = { '0xa', '0x7', '0xa', '0x3', '0x47', '0x50', '0x55', '0x10', '0x0', '0x40', '0x1' };
        uint8_t config[15] = { '0xa', '0x7', '0xa', '0x3', '0x47', '0x50', '0x55', '0x10', '0x0', '0x10', '0x1', '0x28', '0x1', '0x40', '0x1' };

        CStatus configStatus;
        TF_SetConfig(sessionOpts, (void*)config, 15, configStatus.ptr);
        if (configStatus.failure()) {
            std::cerr << "Failed to config tensorflow:" << '\n';
            configStatus.dump_error();
            return nullptr;
        }

        auto opts = tf_obj_unique_ptr(sessionOpts);
        session->session = tf_obj_unique_ptr(TF_NewSession(graph.get(), opts.get(), status.ptr));

        GPUDeviceName(session->session.get());
    }

    if(status.failure()){
        return nullptr;
    }
    assert(session);

    graph.swap(session->graph);
    session->inputs = {input_op, 0};
    session->outputs = {output_op, 0};

    return session.release();
}

/**
 * Deallocator for TF_Buffer data.
 * @tparam T
 * @param data
 * @param length
 */
template<class T> static void free_cpp_array(void* data, size_t length){
    delete []((T *)data);
}

/**
 * Deallocator for TF_NewTensor data.
 * @tparam T
 * @param data
 * @param length
 * @param arg
 */
template<class T> static void cpp_array_deallocator(void* data, size_t length, void* arg){
    delete []((T *)data);
}

static void none_deallocator(void* data, size_t length, void* arg) {
	
}

/**
 * Read the entire content of a file and return it as a TF_Buffer.
 * @param file: The file to be loaded.
 * @return
 */
static TF_Buffer* read_tf_buffer_from_file(const char* file) {
    std::ifstream t(file, std::ifstream::binary);
    t.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    t.seekg(0, std::ios::end);
    size_t size = t.tellg();
    auto data = std::make_unique<char[]>(size);
    t.seekg(0);
    t.read(data.get(), size);

    TF_Buffer *buf = TF_NewBuffer();
    buf->data = data.release();
    buf->length = size;
    buf->data_deallocator = free_cpp_array<char>;
    return buf;
}

#define MY_TENSOR_SHAPE_MAX_DIM 16
struct TensorShape{
    int64_t values[MY_TENSOR_SHAPE_MAX_DIM];
    int dim;

    int64_t size()const{
        assert(dim>=0);
        int64_t v=1;
        for(int i=0;i<dim;i++)v*=values[i];
        return v;
    }
};
