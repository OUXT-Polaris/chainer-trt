/*
 * Copyright (c) 2018 Preferred Networks, Inc. All rights reserved.
 */

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <chainer_trt/external/picojson.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <NvInfer.h>
#pragma GCC diagnostic pop

#define IS_TRT5RC              \
    (NV_TENSORRT_MAJOR == 5 && \
     (NV_TENSORRT_MINOR == 0 && NV_TENSORRT_PATCH < 2))

namespace chainer_trt {
namespace plugin {
    using network_def = std::shared_ptr<nvinfer1::INetworkDefinition>;
    using name_tensor_map = std::map<std::string, nvinfer1::ITensor*>;
    using builder_func = std::function<nvinfer1::ILayer*(
      network_def, const picojson::object&, nvinfer1::DataType,
      const name_tensor_map&, const std::string&)>;
    using deserializer_func =
      std::function<nvinfer1::IPluginExt*(const void*, int)>;

    class plugin_factory : public nvinfer1::IPluginFactory {

        std::map<std::string, builder_func> plugin_builders;
        std::map<std::string, deserializer_func> plugin_deserializers;

    public:
        plugin_factory();

        // For network deserializer
        // (called from TensorRT runtime when loading a built engine file)
        nvinfer1::IPlugin* createPlugin(const char* layerName,
                                        const void* serialData,
                                        size_t serialLength) override;

        // Call this method to let chainer-trt know
        // your external plugins
        void add_builder_deserializer(const std::string& type,
                                      builder_func builder,
                                      deserializer_func deserializer) {
            plugin_builders[type] = builder;
            plugin_deserializers[type] = deserializer;
        }

        bool is_registered(const std::string& type) const {
            return plugin_builders.find(type) != plugin_builders.end();
        }

        nvinfer1::ILayer* build_plugin(network_def network,
                                       const picojson::object& layer_params,
                                       nvinfer1::DataType dt,
                                       const name_tensor_map& tensor_names,
                                       const std::string& model_dir);
    };

    template <class T>
    class plugin_base : public nvinfer1::IPluginExt {
    protected:
        nvinfer1::PluginFormat plugin_format = nvinfer1::PluginFormat::kNCHW;
        nvinfer1::DataType data_type = nvinfer1::DataType::kFLOAT;

    public:
        plugin_base() {}

        static T* deserialize(const void* serialData, size_t serialLength) {
            return new T(serialData, serialLength);
        }

        // Default: Plugin should support NCHW-ordered FP32/FP16
        bool supportsFormat(nvinfer1::DataType type,
                            nvinfer1::PluginFormat format) const override {
            if(format != nvinfer1::PluginFormat::kNCHW)
                return false;
            if(type != nvinfer1::DataType::kFLOAT &&
               type != nvinfer1::DataType::kHALF)
                return false;
            return true;
        }

        void configureWithFormat(const nvinfer1::Dims* inputDims, int nbInputs,
                                 const nvinfer1::Dims* outputDims,
                                 int nbOutputs, nvinfer1::DataType type,
                                 nvinfer1::PluginFormat format,
                                 int maxBatchSize) override {
            (void)inputDims;
            (void)nbInputs;
            (void)outputDims;
            (void)nbOutputs;
            (void)maxBatchSize;

            data_type = type;
            plugin_format = format;

            if(type != nvinfer1::DataType::kFLOAT &&
               type != nvinfer1::DataType::kHALF)
                throw std::runtime_error(
                  "Invalid DataType is specified to enqueue");
        }

        int getNbOutputs() const override { return 1; }

        int initialize() override { return 0; }

        void terminate() override {}

        size_t getWorkspaceSize(int) const override { return 0; }

#if IS_TRT5RC
        // workardounds
        // TensorRT on DrivePX is not yet GA,
        // which requires the following interfaces to be implemented

        const char* getPluginType() const override { return typeid(T).name(); }

        const char* getPluginVersion() const override { return "1.0.0"; }

        void destroy() override { delete this; }

        IPluginExt* clone() const override {
            // Clone object through serialization
            const size_t s = ((T*)this)->getSerializationSize();
            if(s == 0) {
                std::ostringstream ost;
                ost << "Error: serialization size of " << typeid(T).name();
                ost << " reported by getSerializationSize() is 0.";
                ost << " Something is wrong.";
                throw std::runtime_error(ost.str());
            }

            std::vector<unsigned char> buf(s, 0);

            // serialize
            ((T*)this)->serialize((void*)buf.data());

            // create new instance from the serialization
            return new T(buf.data(), s);
        }
#endif
    };
}
}
