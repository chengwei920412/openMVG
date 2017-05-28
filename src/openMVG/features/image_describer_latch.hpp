// Copyright (c) 2015 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENMVG_FEATURES_LATCH_IMAGE_DESCRIBER_HPP
#define OPENMVG_FEATURES_LATCH_IMAGE_DESCRIBER_HPP

#include <iostream>
#include <numeric>

#include "openMVG/features/image_describer.hpp"
#include "openMVG/features/regions_factory.hpp"
#include "openMVG/image/image_container.hpp"
#include <cereal/cereal.hpp>

#ifdef OPENMVG_USE_CUDA
#include "third_party/koral/KORAL.h"

namespace openMVG {
namespace features {

enum ELATCH_DESCRIPTOR
{
  LATCH_BINARY
};

class LATCH_Image_describer : public Image_describer
{
public:

  struct Params
  {
    Params(
      ELATCH_DESCRIPTOR eLatchDescriptor = LATCH_BINARY
    ):eLatchDescriptor_(eLatchDescriptor)
    {
    }

    template<class Archive>
    void serialize(Archive & ar)
    {
      ar(eLatchDescriptor_, scale_factor_, scale_levels_, KFAST_threshold_);
    }

    // Parameters
    ELATCH_DESCRIPTOR eLatchDescriptor_;
    float scale_factor_ = 1.2f;
    uint8_t scale_levels_ = 8;
    uint8_t KFAST_threshold_ = 60;
  };


  LATCH_Image_describer(
    const Params & params = std::move(Params())
  ):Image_describer(), params_(params), koral_(params_.scale_factor_, params_.scale_levels_)
  {
  }

  // Don't need to really define this for the LATCH class yet, until more descriptors come out.
  bool Set_configuration_preset(EDESCRIBER_PRESET preset) override
  {
    return true;
  }

  /**
  @brief Detect regions on the image and compute their attributes (description)
  @param regions The detected regions and attributes (the caller must delete the allocated data)
  @param mask 8-bit gray image for keypoint filtering (optional).
     Non-zero values depict the region of interest.
  */
  std::unique_ptr<Regions> Describe
  (
    const image::Image<unsigned char>& image,
    const image::Image<unsigned char> * mask = nullptr
  ) override
  {
    koral_.go(image.data(), image.cols(), image.rows(), params_.KFAST_threshold_);
    const auto & kpts = koral_.kps;
    const auto & descriptors = koral_.desc;

    auto regions = std::unique_ptr<AKAZE_Binary_Regions>(new AKAZE_Binary_Regions);
    switch (params_.eLatchDescriptor_)
    {
      case LATCH_BINARY:
      {
        // Build alias to cached data
        regions->Features().resize(kpts.size());
        regions->Descriptors().resize(kpts.size());

        for (int i = 0; i < static_cast<int>(kpts.size()); ++i)
        {
          const Keypoint ptLatch = kpts[i];

          // Feature masking
          if (mask)
          {
            const image::Image<unsigned char> & maskIma = *mask;
            if (maskIma(ptLatch.y, ptLatch.x) == 0)
              continue;
          }
          const float scale = static_cast<float>(std::pow(1.2f, ptLatch.scale));
          regions->Features()[i] = {
            scale * static_cast<float>(ptLatch.x),
            scale * static_cast<float>(ptLatch.y),
            7.0f * scale,
            ptLatch.angle
          };
          std::memcpy(&(regions->Descriptors()[i]), &(descriptors[i * 8]), 8 * sizeof(uint64_t));
        }
      }
      break;
    }
    return regions;
  };

  /// Allocate Regions type depending of the Image_describer
  std::unique_ptr<Regions> Allocate() const override
  {
    return std::unique_ptr<AKAZE_Binary_Regions>(new AKAZE_Binary_Regions);
  }

  template<class Archive>
  void serialize(Archive & ar)
  {
    ar(cereal::make_nvp("params", params_),
       cereal::make_nvp("bOrientation", bOrientation_));
  }

private:
  Params params_;
  bool bOrientation_;
  KORAL koral_;
};

} // namespace features
} // namespace openMVG

#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/json.hpp>
CEREAL_REGISTER_TYPE_WITH_NAME(openMVG::features::LATCH_Image_describer, "LATCH_Image_describer");
CEREAL_REGISTER_POLYMORPHIC_RELATION(openMVG::features::Image_describer, openMVG::features::LATCH_Image_describer)
#endif // OPENMVG_FEATURES_LATCH_IMAGE_DESCRIBER_HPP

#endif // OpenMVG_USE_CUDA
