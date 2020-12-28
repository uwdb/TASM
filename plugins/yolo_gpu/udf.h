#ifndef LIGHTDB_YOLO_GPU_LIBRARY_H
#define LIGHTDB_YOLO_GPU_LIBRARY_H

#include "Functor.h"

#include <yolo_v2_class.hpp>

class YOLOGPU: public lightdb::functor::unaryfunctor {
    class GPU: public lightdb::functor::unaryfunction {
    public:
        GPU();
        GPU(const std::string &configuration_path,
            const std::string &weights_path,
            const std::string &names_path,
            float threshold=0.2f)
                : lightdb::functor::unaryfunction(lightdb::physical::DeviceType::GPU,
                        lightdb::Codec::boxes(),
                        true),
                    detector_(new Detector(configuration_path, weights_path)),
                    objectNames_(objects_names_from_file(names_path)),
                    width_(0),
                    height_(0),
                    yuvHandle_(0),
                    bgrHandle_(0),
                    resizedHandle_(0),
                    packedFloatHandle_(0),
                    resizedPlanarHandle_(0)
        {}

        ~GPU() {
            deallocate();
        }

        lightdb::shared_reference<lightdb::LightField> operator()(lightdb::LightField &field) override;

    private:
        static std::vector<std::string> objects_names_from_file(const std::string &filename);
        void allocate(unsigned int height, unsigned int width);
        void deallocate();
        void saveToNpy();

        std::shared_ptr<Detector> detector_;
        std::vector<std::string> objectNames_;

        static const int inputWidth_ = 416;
        static const int inputHeight_ = 416;
        static const int channels_ = 3;
        unsigned int width_;
        unsigned int height_;
        CUdeviceptr yuvHandle_;
        size_t yuvPitch_;
        CUdeviceptr bgrHandle_;
        size_t bgrPitch_;
        CUdeviceptr resizedHandle_;
        size_t resizedPitch_;
        CUdeviceptr packedFloatHandle_;
        size_t packedFloatPitch_;
        CUdeviceptr resizedPlanarHandle_;
    };
public:
    YOLOGPU() : lightdb::functor::unaryfunctor("YOLOGPU", GPU()) { }
};

#endif //LIGHTDB_YOLO_GPU_LIBRARY_H
