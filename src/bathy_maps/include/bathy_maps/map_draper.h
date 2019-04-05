/* Copyright 2018 Nils Bore (nbore@kth.se)
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MAP_DRAPER_H
#define MAP_DRAPER_H

#include <bathy_maps/base_draper.h>
#include <bathy_maps/patch_views.h>
#include <bathy_maps/sss_map_image.h>

struct MapDraper : public BaseDraper {
public:

    using BoundsT = Eigen::Matrix2d;

protected:

    //BoundsT bounds;
    double resolution;
    std::function<void(sss_map_image)> save_callback;
    sss_map_image::ImagesT map_images;
    sss_map_image_builder map_image_builder;
    //Eigen::MatrixXd draping_vis_texture;
    bool store_map_images;

public:
    
    static void default_callback(const sss_map_image&) {}

    void set_image_callback(const std::function<void(sss_map_image)>& callback) { save_callback = callback; }
    void set_resolution(double new_resolution);
    void set_store_map_images(bool store) { store_map_images = store; }

    MapDraper(const Eigen::MatrixXd& V1, const Eigen::MatrixXi& F1,
              const xtf_data::xtf_sss_ping::PingsT& pings,
              const BoundsT& bounds,
              const csv_data::csv_asvp_sound_speed::EntriesT& sound_speeds);

    bool callback_pre_draw(igl::opengl::glfw::Viewer& viewer);
    sss_map_image::ImagesT get_images();
};

sss_map_image::ImagesT drape_maps(const Eigen::MatrixXd& V, const Eigen::MatrixXi& F,
                                  const MapDraper::BoundsT& bounds, const xtf_data::xtf_sss_ping::PingsT& pings,
                                  const csv_data::csv_asvp_sound_speed::EntriesT& sound_speeds, double sensor_yaw,
                                  double resolution, const std::function<void(sss_map_image)>& save_callback);

#endif // MAP_DRAPER_H
