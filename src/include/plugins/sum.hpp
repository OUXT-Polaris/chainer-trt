/*
 * Copyright (c) 2018 Preferred Networks, Inc. All rights reserved.
 */

#pragma once

#include <chainer_trt/plugin.hpp>

namespace chainer_trt {
namespace plugin {
    // Applies argmax to channel (1st dim) of input
    class sum : public plugin_base<sum> {
        nvinfer1::Dims input_dims;

    public:
        sum(nvinfer1::Dims _dims);
        sum(const void* buf, size_t size);

        static nvinfer1::ILayer*
        build_layer(network_def network, const picojson::object& layer_params,
                    nvinfer1::DataType dt, const name_tensor_map& tensor_names,
                    const std::string& model_dir);

        size_t getSerializationSize() override;
        nvinfer1::Dims getOutputDimensions(int index,
                                           const nvinfer1::Dims* inputs,
                                           int nbInputDims) override;

        int enqueue(int batchSize, const void* const* inputs, void** outputs,
                    void* workspace, cudaStream_t stream) override;

        void serialize(void* buffer) override;

        const char* get_plugin_type() const override {
            return "chainer_trt_sum";
        }

        const char* get_plugin_version() const override { return "1.0.0"; }
    };
}
}
