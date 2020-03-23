//
// Created by Maureen Daum on 2019-06-21.
//

#include "MetadataLightField.h"

#define ASSERT_SQLITE_OK(i) (assert(i == SQLITE_OK))

namespace lightdb::associations {
    static const std::string traffic_2k_tracking_db = "/home/maureen/visualroad/2k-short/traffic-001-with-tracking-info-corrected.db";
    static const std::string traffic_2k_tracking_db_no_id = "/home/maureen/visualroad/2k-short/traffic-001-with-tracking-info-corrected-no-id.db";
    static const std::string traffic_2k_db = "/home/maureen/visualroad/2k-short/traffic-001.db";

    static const std::unordered_map<std::string, std::string> VideoPathToLabelsPath(
            {
                    {"traffic-2k", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked3x3", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked3x3-2", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked-gop60", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked-gop120", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked-gop240", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked-gop300", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-single-tile", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-001", traffic_2k_db},
                    {"traffic-2k-001-cracked-grouping-extent-entire-video", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-grouping-extent-entire-video", traffic_2k_db},
                    {"traffic-2k-001-2x2", traffic_2k_db},
                    {"traffic-2k-001-3x3", traffic_2k_db},
                    {"traffic-2k-001-4x4", traffic_2k_db},
                    {"traffic-2k-001-5x5", traffic_2k_db},
                    {"traffic-2k-001-6x6", traffic_2k_db},
                    {"traffic-2k-001-7x7", traffic_2k_db},
                    {"traffic-2k-001-7x8", traffic_2k_db},
                    {"traffic-2k-001-7x9", traffic_2k_db},
                    {"traffic-2k-001-7x10", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-grouping-extent-duration30", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-grouping-extent-duration60", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-grouping-extent-duration450", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-grouping-extent-duration900", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-grouping-extent-duration1350", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-smalltiles-duration450", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-smalltiles-duration900", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-smalltiles-duration1350", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-smalltiles-duration30", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-smalltiles-duration60", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-smalltiles-duration120", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-smalltiles-duration300", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-smalltiles-duration900", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-smalltiles-duration1800", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-smalltiles-duration9000", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-smalltiles-duration18000", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-grouping-extent-duration300", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-grouping-extent-duration900", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-grouping-extent-duration1800", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-grouping-extent-duration9000", traffic_2k_db},
                    {"traffic-2k-001-cracked-car-grouping-extent-duration18000", traffic_2k_db},
                    {"traffic-2k-001-cracked-pedestrian-grouping-extent-entire-video", traffic_2k_db},
                    {"traffic-2k-001-cracked-pedestrian-grouping-extent-duration30", traffic_2k_db},
                    {"traffic-2k-001-cracked-pedestrian-grouping-extent-duration60", traffic_2k_db},
                    {"traffic-2k-001-cracked-pedestrian-smalltiles-duration30", traffic_2k_db},
                    {"traffic-2k-001-cracked-pedestrian-smalltiles-duration60", traffic_2k_db},
                    {"traffic-2k-001-cracked-pedestrian-smalltiles-duration120", traffic_2k_db},
                    // 4k-002
                    {"traffic-4k-002", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-layoutduration60-car", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-layoutduration30-car", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-groupingextent-layoutduration60-car", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-groupingextent-layoutduration30-car", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-layoutduration60-pedestrian", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-groupingextent-layoutduration60-pedestrian", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-single-tile", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-alignedTo32-layoutduration60-car", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-alignedTo32-layoutduration30-car", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-grouping-extent-entire-video", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-grouping-extent-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-grouping-extent-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-grouping-extent-duration450", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-grouping-extent-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-grouping-extent-duration1350", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-smalltiles-duration450", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-smalltiles-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-smalltiles-duration1350", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-2x2", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-3x3", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-4x4", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-5x5", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-6x6", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-7x7", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-7x8", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-7x9", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-7x10", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-smalltiles-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-smalltiles-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-smalltiles-duration120", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-smalltiles-duration300", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-smalltiles-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-smalltiles-duration1800", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-smalltiles-duration9000", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-smalltiles-duration18000", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-grouping-extent-duration300", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-grouping-extent-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-grouping-extent-duration1800", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-grouping-extent-duration9000", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-car-grouping-extent-duration18000", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-pedestrian-grouping-extent-entire-video", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-pedestrian-grouping-extent-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-pedestrian-grouping-extent-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-pedestrian-smalltiles-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-pedestrian-smalltiles-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-pedestrian-smalltiles-duration120", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-ds1k", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-car-grouping-extent-entire-video", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-grouping-extent-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-grouping-extent-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-car-grouping-extent-duration450", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-car-grouping-extent-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-car-grouping-extent-duration1350", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-car-smalltiles-duration450", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-car-smalltiles-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-car-smalltiles-duration1350", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-smalltiles-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-smalltiles-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-smalltiles-duration120", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-2x2", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-3x3", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-4x4", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-5x5", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-pedestrian-grouping-extent-entire-video", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-pedestrian-grouping-extent-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-pedestrian-grouping-extent-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-pedestrian-smalltiles-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-pedestrian-smalltiles-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds1k-cracked-pedestrian-smalltiles-duration120", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds2k", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-grouping-extent-entire-video", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-grouping-extent-entire-video", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-grouping-extent-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-grouping-extent-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-grouping-extent-duration450", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-grouping-extent-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-grouping-extent-duration1350", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-smalltiles-duration450", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-smalltiles-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-smalltiles-duration1350", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-2x2", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-3x3", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-4x4", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-5x5", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-6x6", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-7x7", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-7x8", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-7x9", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-7x10", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-smalltiles-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-smalltiles-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-smalltiles-duration120", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-smalltiles-duration300", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-smalltiles-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-smalltiles-duration1800", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-smalltiles-duration9000", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-smalltiles-duration18000", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-grouping-extent-duration300", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-grouping-extent-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-grouping-extent-duration1800", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-grouping-extent-duration9000", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-car-grouping-extent-duration18000", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-pedestrian-grouping-extent-entire-video", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-pedestrian-grouping-extent-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-pedestrian-grouping-extent-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-pedestrian-smalltiles-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-pedestrian-smalltiles-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-ds2k-cracked-pedestrian-smalltiles-duration120", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    // 4k-000
                    {"traffic-4k-000", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-grouping-extent-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-grouping-extent-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-grouping-extent-duration450", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-grouping-extent-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-grouping-extent-duration1350", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-smalltiles-duration450", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-smalltiles-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-smalltiles-duration1350", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-grouping-extent-entire-video", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-smalltiles-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-smalltiles-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-smalltiles-duration120", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-smalltiles-duration300", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-smalltiles-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-smalltiles-duration1800", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-smalltiles-duration9000", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-smalltiles-duration18000", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-grouping-extent-duration300", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-grouping-extent-duration900", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-grouping-extent-duration1800", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-grouping-extent-duration9000", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-car-grouping-extent-duration18000", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-pedestrian-grouping-extent-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-pedestrian-grouping-extent-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-pedestrian-grouping-extent-entire-video", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-pedestrian-smalltiles-duration30", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-pedestrian-smalltiles-duration60", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-pedestrian-smalltiles-duration120", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-2x2", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-3x3", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-4x4", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-5x5", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-6x6", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-7x7", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-7x8", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-7x9", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-7x10", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    // 4k-002 downsampled
                    {"traffic-4k-002-downsampled-to-2k", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-downsampled2k-cracked-layoutduration30-car", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-downsampled2k-cracked-layoutduration60-car", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-downsampled2k-cracked-groupingextent-layoutduration30-car", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    {"traffic-4k-002-downsampled2k-cracked-groupingextent-layoutduration60-car", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"},
                    // short-1k-2
                    {"traffic-1k-002", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-car-grouping-extent-entire-video", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-grouping-extent-duration30", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-grouping-extent-duration60", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-car-grouping-extent-duration450", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-car-grouping-extent-duration900", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-car-grouping-extent-duration1350", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-car-smalltiles-duration450", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-car-smalltiles-duration900", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-car-smalltiles-duration1350", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-smalltiles-duration30", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-smalltiles-duration60", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-smalltiles-duration120", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-2x2", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-3x3", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-4x4", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-5x5", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-pedestrian-grouping-extent-entire-video", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-pedestrian-grouping-extent-duration30", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-pedestrian-grouping-extent-duration60", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-pedestrian-smalltiles-duration30", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-pedestrian-smalltiles-duration60", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    {"traffic-1k-002-cracked-pedestrian-smalltiles-duration120", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},
                    // car-pov-2k-000
                    {"car-pov-2k-000", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-cracked-grouping-extent-entire-video", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-cracked-grouping-extent-duration30", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-cracked-grouping-extent-duration60", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-cracked-smalltiles-duration30", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-cracked-smalltiles-duration60", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-cracked-smalltiles-duration120", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-3x3", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-cracked-smalltiles-fixed-duration30", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    // car-pov-2k-000-shortened
                    {"car-pov-2k-000-shortened", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-2x2", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-3x3", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-4x4", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-5x5", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-6x6", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-7x7", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-7x8", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-7x9", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-7x10", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-grouping-extent-entire-video", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-grouping-extent-duration30", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-grouping-extent-duration60", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-grouping-extent-duration240", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-grouping-extent-duration480", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-grouping-extent-duration720", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-smalltiles-duration240", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-smalltiles-duration480", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-smalltiles-duration720", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-smalltiles-duration30", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-smalltiles-duration60", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-smalltiles-duration120", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-smalltiles-duration300", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-smalltiles-duration900", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-smalltiles-duration1800", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-smalltiles-duration9000", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-smalltiles-duration18000", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-grouping-extent-duration300", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-grouping-extent-duration900", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-grouping-extent-duration1800", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-grouping-extent-duration9000", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-car-grouping-extent-duration18000", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-pedestrian-grouping-extent-entire-video", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-pedestrian-grouping-extent-duration30", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-pedestrian-grouping-extent-duration60", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-pedestrian-smalltiles-duration30", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-pedestrian-smalltiles-duration60", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    {"car-pov-2k-000-shortened-cracked-pedestrian-smalltiles-duration120", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},
                    // car-pov-2k-001-shortened
                    {"car-pov-2k-001-shortened", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-2x2", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-3x3", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-4x4", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-5x5", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-6x6", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-7x7", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-7x8", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-7x9", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-7x10", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-grouping-extent-entire-video", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-grouping-extent-duration30", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-grouping-extent-duration60", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-grouping-extent-duration270", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-grouping-extent-duration540", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-grouping-extent-duration810", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-smalltiles-duration270", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-smalltiles-duration540", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-smalltiles-duration810", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-smalltiles-duration30", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-smalltiles-duration60", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-smalltiles-duration120", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-smalltiles-duration300", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-smalltiles-duration900", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-smalltiles-duration1800", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-smalltiles-duration9000", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-smalltiles-duration18000", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-grouping-extent-duration300", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-grouping-extent-duration900", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-grouping-extent-duration1800", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-grouping-extent-duration9000", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-car-grouping-extent-duration18000", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-pedestrian-grouping-extent-entire-video", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-pedestrian-grouping-extent-duration30", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-pedestrian-grouping-extent-duration60", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-pedestrian-smalltiles-duration30", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-pedestrian-smalltiles-duration60", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-shortened-cracked-pedestrian-smalltiles-duration120", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    // car-pov-2k-001
                    {"car-pov-2k-001", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-3x3", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-4x4", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-5x5", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-cracked-grouping-extent-entire-video", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-cracked-grouping-extent-duration30", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-cracked-grouping-extent-duration60", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-cracked-smalltiles-duration30", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-cracked-smalltiles-duration60", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    {"car-pov-2k-001-cracked-smalltiles-duration120", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},
                    // coral-reef
                    {"coral-reef", "/home/maureen/noscope_videos/coral-reef-short.db"},
                    {"coral-reef-3x3", "/home/maureen/noscope_videos/coral-reef-short.db"},
                    {"coral-reef-4x4", "/home/maureen/noscope_videos/coral-reef-short.db"},
                    {"coral-reef-5x5", "/home/maureen/noscope_videos/coral-reef-short.db"},
                    {"coral-reef-cracked-person-smalltiles-duration30", "/home/maureen/noscope_videos/coral-reef-short.db"},
                    {"coral-reef-cracked-person-smalltiles-duration60", "/home/maureen/noscope_videos/coral-reef-short.db"},
                    {"coral-reef-cracked-person-smalltiles-duration120", "/home/maureen/noscope_videos/coral-reef-short.db"},
                    {"coral-reef-cracked-person-grouping-extent-duration30", "/home/maureen/noscope_videos/coral-reef-short.db"},
                    {"coral-reef-cracked-person-grouping-extent-duration60", "/home/maureen/noscope_videos/coral-reef-short.db"},
                    {"coral-reef-cracked-person-grouping-extent-entire-video", "/home/maureen/noscope_videos/coral-reef-short.db"},
                    // CDVL
                    // Aerial
                    {"aerial", "/home/maureen/cdvl/labels/aerial.db"},
                    {"aerial-3x3", "/home/maureen/cdvl/labels/aerial.db"},
                    {"aerial-4x4", "/home/maureen/cdvl/labels/aerial.db"},
                    {"aerial-5x5", "/home/maureen/cdvl/labels/aerial.db"},
                    {"aerial-cracked-car-smalltiles-duration29", "/home/maureen/cdvl/labels/aerial.db"},
                    {"aerial-cracked-car-smalltiles-duration58", "/home/maureen/cdvl/labels/aerial.db"},
                    {"aerial-cracked-car-smalltiles-duration87", "/home/maureen/cdvl/labels/aerial.db"},
                    {"aerial-cracked-car-grouping-extent-duration29", "/home/maureen/cdvl/labels/aerial.db"},
                    {"aerial-cracked-car-grouping-extent-duration58", "/home/maureen/cdvl/labels/aerial.db"},
                    {"aerial-cracked-car-grouping-extent-entire-video", "/home/maureen/cdvl/labels/aerial.db"},

                    // Busy
                    {"busy", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-3x3", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-4x4", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-5x5", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-bus-smalltiles-duration23", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-bus-smalltiles-duration46", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-bus-smalltiles-duration69", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-bus-grouping-extent-duration23", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-bus-grouping-extent-duration46", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-bus-grouping-extent-entire-video", "/home/maureen/cdvl/labels/busy.db"},

                    {"busy-cracked-car-smalltiles-duration23", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-car-smalltiles-duration46", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-car-smalltiles-duration69", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-car-grouping-extent-duration23", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-car-grouping-extent-duration46", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-car-grouping-extent-entire-video", "/home/maureen/cdvl/labels/busy.db"},

                    {"busy-cracked-person-smalltiles-duration23", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-person-smalltiles-duration46", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-person-smalltiles-duration69", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-person-grouping-extent-duration23", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-person-grouping-extent-duration46", "/home/maureen/cdvl/labels/busy.db"},
                    {"busy-cracked-person-grouping-extent-entire-video", "/home/maureen/cdvl/labels/busy.db"},

                    // Childs
                    {"childs", "/home/maureen/cdvl/labels/childs.db"},
                    {"childs-3x3", "/home/maureen/cdvl/labels/childs.db"},
                    {"childs-4x4", "/home/maureen/cdvl/labels/childs.db"},
                    {"childs-5x5", "/home/maureen/cdvl/labels/childs.db"},
                    {"childs-cracked-person-smalltiles-duration23", "/home/maureen/cdvl/labels/childs.db"},
                    {"childs-cracked-person-smalltiles-duration46", "/home/maureen/cdvl/labels/childs.db"},
                    {"childs-cracked-person-smalltiles-duration69", "/home/maureen/cdvl/labels/childs.db"},
                    {"childs-cracked-person-grouping-extent-duration23", "/home/maureen/cdvl/labels/childs.db"},
                    {"childs-cracked-person-grouping-extent-duration46", "/home/maureen/cdvl/labels/childs.db"},
                    {"childs-cracked-person-grouping-extent-entire-video", "/home/maureen/cdvl/labels/childs.db"},

                    // Nature
                    {"nature", "/home/maureen/cdvl/labels/nature.db"},
                    {"nature-3x3", "/home/maureen/cdvl/labels/nature.db"},
                    {"nature-4x4", "/home/maureen/cdvl/labels/nature.db"},
                    {"nature-5x5", "/home/maureen/cdvl/labels/nature.db"},
                    {"nature-cracked-bird-smalltiles-duration23", "/home/maureen/cdvl/labels/nature.db"},
                    {"nature-cracked-bird-smalltiles-duration46", "/home/maureen/cdvl/labels/nature.db"},
                    {"nature-cracked-bird-smalltiles-duration69", "/home/maureen/cdvl/labels/nature.db"},
                    {"nature-cracked-bird-grouping-extent-duration23", "/home/maureen/cdvl/labels/nature.db"},
                    {"nature-cracked-bird-grouping-extent-duration46", "/home/maureen/cdvl/labels/nature.db"},
                    {"nature-cracked-bird-grouping-extent-entire-video", "/home/maureen/cdvl/labels/nature.db"},

                    // elfuente_full
                    {"elfuente_full_correctfps", "/home/maureen/cdvl/labels/elfuente_full.db"},
                    {"elfuente_full-3x3", "/home/maureen/cdvl/labels/elfuente_full.db"},
                    {"elfuente_full-4x4", "/home/maureen/cdvl/labels/elfuente_full.db"},
                    {"elfuente_full-5x5", "/home/maureen/cdvl/labels/elfuente_full.db"},
                    {"elfuente_full-cracked-person-smalltiles-duration59", "/home/maureen/cdvl/labels/elfuente_full.db"},
                    {"elfuente_full-cracked-person-smalltiles-duration118", "/home/maureen/cdvl/labels/elfuente_full.db"},
                    {"elfuente_full-cracked-person-smalltiles-duration177", "/home/maureen/cdvl/labels/elfuente_full.db"},
                    {"elfuente_full-cracked-person-grouping-extent-duration59", "/home/maureen/cdvl/labels/elfuente_full.db"},
                    {"elfuente_full-cracked-person-grouping-extent-duration118", "/home/maureen/cdvl/labels/elfuente_full.db"},
                    {"elfuente_full-cracked-person-grouping-extent-entire-video", "/home/maureen/cdvl/labels/elfuente_full.db"},

                    // NFLX_DATASET
                    // birdsincage
                    {"birdsincage", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-2x2", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-3x3", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-4x4", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-5x5", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-6x6", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-7x7", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-7x8", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-7x9", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-7x10", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-cracked-bird-grouping-extent-duration30", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-cracked-bird-grouping-extent-duration60", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-cracked-bird-grouping-extent-duration90", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-cracked-bird-grouping-extent-duration120", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-cracked-bird-grouping-extent-duration150", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-cracked-bird-grouping-extent-entire-video", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-cracked-bird-smalltiles-duration30", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-cracked-bird-smalltiles-duration60", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-cracked-bird-smalltiles-duration90", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-cracked-bird-smalltiles-duration120", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},
                    {"birdsincage-cracked-bird-smalltiles-duration150", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},

                    // crowdrun
                    {"crowdrun", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-2x2", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-3x3", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-4x4", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-5x5", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-6x6", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-7x7", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-7x8", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-7x9", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-7x10", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-cracked-person-grouping-extent-duration25", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-cracked-person-grouping-extent-duration50", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-cracked-person-grouping-extent-duration75", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-cracked-person-grouping-extent-duration100", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-cracked-person-grouping-extent-duration125", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-cracked-person-grouping-extent-entire-video", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-cracked-person-smalltiles-duration25", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-cracked-person-smalltiles-duration50", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-cracked-person-smalltiles-duration75", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-cracked-person-smalltiles-duration100", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},
                    {"crowdrun-cracked-person-smalltiles-duration125", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},

                    // elfuente1
                    {"elfuente1", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-2x2", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-3x3", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-4x4", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-5x5", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-6x6", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-7x7", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-7x8", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-7x9", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-7x10", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-cracked-person-grouping-extent-duration30", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-cracked-person-grouping-extent-duration60", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-cracked-person-grouping-extent-duration90", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-cracked-person-grouping-extent-duration120", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-cracked-person-grouping-extent-duration150", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-cracked-person-grouping-extent-entire-video", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-cracked-person-smalltiles-duration30", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-cracked-person-smalltiles-duration60", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-cracked-person-smalltiles-duration90", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-cracked-person-smalltiles-duration120", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},
                    {"elfuente1-cracked-person-smalltiles-duration150", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},

                    // elfuente2
                    {"elfuente2", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-2x2", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-3x3", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-4x4", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-5x5", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-6x6", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-7x7", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-7x8", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-7x9", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-7x10", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-cracked-person-grouping-extent-duration30", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-cracked-person-grouping-extent-duration60", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-cracked-person-grouping-extent-duration90", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-cracked-person-grouping-extent-duration120", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-cracked-person-grouping-extent-duration150", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-cracked-person-grouping-extent-entire-video", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-cracked-person-smalltiles-duration30", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-cracked-person-smalltiles-duration60", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-cracked-person-smalltiles-duration90", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-cracked-person-smalltiles-duration120", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2-cracked-person-smalltiles-duration150", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},
                    {"elfuente2_2-cracked-person-entire-video", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},

                    // oldtown
                    {"oldtown", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-2x2", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-3x3", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-4x4", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-5x5", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-6x6", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-7x7", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-7x8", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-7x9", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-7x10", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-cracked-car-grouping-extent-duration25", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-cracked-car-grouping-extent-duration50", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-cracked-car-grouping-extent-duration75", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-cracked-car-grouping-extent-duration100", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-cracked-car-grouping-extent-duration125", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-cracked-car-grouping-extent-entire-video", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-cracked-car-smalltiles-duration25", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-cracked-car-smalltiles-duration50", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-cracked-car-smalltiles-duration75", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-cracked-car-smalltiles-duration100", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},
                    {"oldtown-cracked-car-smalltiles-duration125", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},

                    // seeking
                    {"seeking", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-2x2", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-3x3", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-4x4", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-5x5", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-6x6", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-7x7", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-7x8", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-7x9", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-7x10", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-cracked-person-grouping-extent-duration25", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-cracked-person-grouping-extent-duration50", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-cracked-person-grouping-extent-duration75", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-cracked-person-grouping-extent-duration100", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-cracked-person-grouping-extent-duration125", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-cracked-person-grouping-extent-entire-video", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-cracked-person-smalltiles-duration25", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-cracked-person-smalltiles-duration50", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-cracked-person-smalltiles-duration75", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-cracked-person-smalltiles-duration100", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},
                    {"seeking-cracked-person-smalltiles-duration125", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},

                    // tennis
                    {"tennis", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-2x2", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-3x3", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-4x4", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-5x5", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-6x6", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-7x7", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-7x8", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-7x9", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-7x10", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-person-grouping-extent-duration24", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-person-grouping-extent-duration48", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-person-grouping-extent-duration72", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-person-grouping-extent-duration96", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-person-grouping-extent-duration120", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-person-grouping-extent-entire-video", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-person-smalltiles-duration24", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-person-smalltiles-duration48", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-person-smalltiles-duration72", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-person-smalltiles-duration96", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-person-smalltiles-duration120", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-sports ball-grouping-extent-duration24", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-sports ball-grouping-extent-duration48", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-sports ball-grouping-extent-entire-video", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-sports ball-smalltiles-duration24", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-sports ball-smalltiles-duration48", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-sports ball-smalltiles-duration72", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-tennis racket-grouping-extent-duration24", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-tennis racket-grouping-extent-duration48", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-tennis racket-grouping-extent-entire-video", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-tennis racket-smalltiles-duration24", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-tennis racket-smalltiles-duration48", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},
                    {"tennis-cracked-tennis racket-smalltiles-duration72", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},

                    // XIPH DATASET
                    {"red_kayak", "/home/maureen/xiph/labels/red_kayak.db"},
                    {"touchdown_pass", "/home/maureen/xiph/labels/touchdown_pass.db"},
                    {"park_joy_2k", "/home/maureen/xiph/labels/park_joy_2k.db"},
                    {"park_joy_4k", "/home/maureen/xiph/labels/park_joy_4k.db"},
                    {"Netflix_ToddlerFountain", "/home/maureen/xiph/labels/Netflix_ToddlerFountain.db"},
                    {"Netflix_Narrator", "/home/maureen/xiph/labels/Netflix_Narrator.db"},
                    {"Netflix_FoodMarket", "/home/maureen/xiph/labels/Netflix_FoodMarket.db"},
                    {"Netflix_FoodMarket2", "/home/maureen/xiph/labels/Netflix_FoodMarket2.db"},
                    {"Netflix_DrivingPOV", "/home/maureen/xiph/labels/Netflix_DrivingPOV.db"},
                    {"Netflix_BoxingPractice", "/home/maureen/xiph/labels/Netflix_BoxingPractice.db"},

                    // MOT16 dataset
                    {"MOT16-01", "/home/maureen/mot16/labels/MOT16-01.db"},
                    {"MOT16-02", "/home/maureen/mot16/labels/MOT16-02.db"},
                    {"MOT16-03", "/home/maureen/mot16/labels/MOT16-03.db"},
                    {"MOT16-04", "/home/maureen/mot16/labels/MOT16-04.db"},
                    {"MOT16-07", "/home/maureen/mot16/labels/MOT16-07.db"},
                    {"MOT16-08", "/home/maureen/mot16/labels/MOT16-08.db"},
                    {"MOT16-09", "/home/maureen/mot16/labels/MOT16-09.db"},
                    {"MOT16-10", "/home/maureen/mot16/labels/MOT16-10.db"},
                    {"MOT16-11", "/home/maureen/mot16/labels/MOT16-11.db"},
                    {"MOT16-12", "/home/maureen/mot16/labels/MOT16-12.db"},
                    {"MOT16-13", "/home/maureen/mot16/labels/MOT16-13.db"},
                    {"MOT16-14", "/home/maureen/mot16/labels/MOT16-14.db"},

                    // NETFLIX dataset
                    {"meridian", "/home/maureen/Netflix/labels/meridian_60fps.db"},
                    {"elfuente_full", "/home/maureen/cdvl/labels/elfuente_full_60fps.db"},
                    {"cosmos", "/home/maureen/Netflix/labels/cosmos.db"},

                    // El Fuente full scenes
                    {"market_all", "/home/maureen/cdvl/elfuente_scenes/labels/market_all.db"},
                    {"narrator", "/home/maureen/cdvl/elfuente_scenes/labels/narrator.db"},
                    {"river_boat", "/home/maureen/cdvl/elfuente_scenes/labels/river_boat.db"},
                    {"square_with_fountain", "/home/maureen/cdvl/elfuente_scenes/labels/square_with_fountain.db"},
                    {"street_night_view", "/home/maureen/cdvl/elfuente_scenes/labels/street_night_view.db"},
            } );
} // namespace lightdb::associations

namespace lightdb::physical {

std::vector<Rectangle> MetadataLightField::allRectangles() {
    std::vector<Rectangle> allRectangles;
    std::for_each(metadata_.begin(), metadata_.end(), [&](std::pair<std::string, std::vector<Rectangle>> labelAndRectangles) {
       allRectangles.insert(allRectangles.end(), labelAndRectangles.second.begin(), labelAndRectangles.second.end());
    });

    return allRectangles;
}
} // namespace lightdb::physical

namespace lightdb::metadata {

    void MetadataManager::openDatabase() {
        std::scoped_lock lock(mutex_);
        int openResult = sqlite3_open_v2(lightdb::associations::VideoPathToLabelsPath.at(videoIdentifier_).c_str(), &db_, SQLITE_OPEN_READONLY, NULL);
        assert(openResult == SQLITE_OK);
    }

    void MetadataManager::closeDatabase() {
        std::scoped_lock lock(mutex_);
        int closeResult = sqlite3_close(db_);
        assert(closeResult == SQLITE_OK);
    }

    void MetadataManager::selectFromMetadataAndApplyFunction(const char* query, std::function<void(sqlite3_stmt*)> resultFn, std::function<void(sqlite3*)> afterOpeningFn) const {
        char *selectFramesStatement = nullptr;
        int size = asprintf(&selectFramesStatement, query, metadataSpecification_.tableName().c_str());
//        if (metadataSpecification_.expectedValue().length()) {
//            size = asprintf(&selectFramesStatement, query,
//                            metadataSpecification_.tableName().c_str(),
//                            metadataSpecification_.columnName().c_str(),
//                            metadataSpecification_.expectedValue().c_str());
//        } else {
//            size = asprintf(&selectFramesStatement, query,
//                            metadataSpecification_.tableName().c_str(),
//                            metadataSpecification_.columnName().c_str(),
//                            std::to_string(metadataSpecification_.expectedIntValue()).c_str());
//        }
        assert(size != -1);
        selectFromMetadataWithoutQuerySubstitutionsAndApplyFunction(selectFramesStatement, size, resultFn, afterOpeningFn);
        free(selectFramesStatement);
    }

    void MetadataManager::selectFromMetadataWithoutQuerySubstitutionsAndApplyFunction(const char* query, unsigned int querySize, std::function<void(sqlite3_stmt*)> resultFn, std::function<void(sqlite3*)> afterOpeningFn) const {
        // Assume that mutex is already held.
        if (afterOpeningFn)
            afterOpeningFn(db_);

        sqlite3_stmt *select;
        auto prepareResult = sqlite3_prepare_v2(db_, query, querySize, &select, nullptr);
        ASSERT_SQLITE_OK(prepareResult);

        // Step and get results.
        int result;
        while ((result = sqlite3_step(select)) == SQLITE_ROW)
            resultFn(select);

        assert(result == SQLITE_DONE);

        sqlite3_finalize(select);
    }

void MetadataManager::selectFromMetadataAndApplyFunctionWithFrameLimits(const char* query, std::function<void(sqlite3_stmt*)> resultFn, std::function<void(sqlite3*)> afterOpeningFn) const {
    // Assume that mutex is already held.
    if (afterOpeningFn)
        afterOpeningFn(db_);

    char *selectFramesStatement = nullptr;
    int size = asprintf(&selectFramesStatement, query, metadataSpecification_.tableName().c_str());
    assert(size != -1);

    sqlite3_stmt *select;
    auto prepareResult = sqlite3_prepare_v2(db_, selectFramesStatement, size, &select, nullptr);
    ASSERT_SQLITE_OK(prepareResult);

    // Step and get results.
    int result;
    while ((result = sqlite3_step(select)) == SQLITE_ROW)
        resultFn(select);

    assert(result == SQLITE_DONE);

    sqlite3_finalize(select);
    free(selectFramesStatement);
}

const std::unordered_set<int> &MetadataManager::framesForMetadata() const {
        std::scoped_lock lock(mutex_);
    if (didSetFramesForMetadata_)
        return framesForMetadata_;

    std::string query = "SELECT DISTINCT frame FROM %s WHERE " + metadataSpecification_.whereClauseConstraints(true);

    selectFromMetadataAndApplyFunctionWithFrameLimits(query.c_str(), [this](sqlite3_stmt *stmt) {
        framesForMetadata_.insert(sqlite3_column_int(stmt, 0));
    });

    didSetFramesForMetadata_ = true;
    return framesForMetadata_;
}

const std::vector<int> &MetadataManager::orderedFramesForMetadata() const {
    {
        std::scoped_lock lock(mutex_);
        if (didSetOrderedFramesForMetadata_)
            return orderedFramesForMetadata_;
    }

    std::unordered_set<int> frames = framesForMetadata();

    std::scoped_lock lock(mutex_);
    orderedFramesForMetadata_.resize(frames.size());
    auto currentIndex = 0;
    for (auto &frame : frames)
        orderedFramesForMetadata_[currentIndex++] = frame;

    std::sort(orderedFramesForMetadata_.begin(), orderedFramesForMetadata_.end());

    didSetOrderedFramesForMetadata_ = true;
    return orderedFramesForMetadata_;
}

const std::unordered_set<int> &MetadataManager::idealKeyframesForMetadata() const {
    {
        std::scoped_lock lock(mutex_);
        if (didSetIdealKeyframesForMetadata_)
            return idealKeyframesForMetadata_;
    }

    auto &orderedFrames = orderedFramesForMetadata();

    {
        std::scoped_lock lock(mutex_);
        didSetIdealKeyframesForMetadata_ = true;
        idealKeyframesForMetadata_ = MetadataManager::idealKeyframesForFrames(orderedFrames);
        return idealKeyframesForMetadata_;
    }
}

std::unordered_set<int> MetadataManager::idealKeyframesForFrames(const std::vector<int> &orderedFrames) {
    if (orderedFrames.empty())
        return {};

    // Find the starts of all sequences.
    auto lastFrame = orderedFrames.front();
    std::unordered_set<int> idealKeyframes;
    idealKeyframes.insert(lastFrame);
    for (auto it = orderedFrames.begin() + 1; it != orderedFrames.end(); it++) {
        if (*it != lastFrame + 1)
            idealKeyframes.insert(*it);

        lastFrame = *it;
    }

    return idealKeyframes;
}

const std::vector<Rectangle> &MetadataManager::rectanglesForFrame(int frame) const {
    static std::vector<Rectangle> emptyVector;
    if ((unsigned int)frame < metadataSpecification_.firstFrame() || (unsigned int)frame >= metadataSpecification_.lastFrame())
        return emptyVector;

    std::scoped_lock lock(mutex_);
    if (frameToRectangles_.count(frame))
        return frameToRectangles_[frame];

    std::string query = "SELECT frame, x, y, width, height FROM %s WHERE (" + metadataSpecification_.whereClauseConstraints(false) + ") and frame = " + std::to_string(frame) + ";";

    selectFromMetadataAndApplyFunction(query.c_str(), [this, frame](sqlite3_stmt *stmt) {
        unsigned int queryFrame = sqlite3_column_int(stmt, 0);
        assert(queryFrame == (unsigned int)frame);
        unsigned int x = sqlite3_column_int(stmt, 1);
        unsigned int y = sqlite3_column_int(stmt, 2);
        unsigned int width = sqlite3_column_int(stmt, 3);
        unsigned int height = sqlite3_column_int(stmt, 4);

        frameToRectangles_[frame].emplace_back(queryFrame, x, y, width, height);
    });
    return frameToRectangles_[frame];
}

std::unique_ptr<std::list<Rectangle>> MetadataManager::rectanglesForFrames(int firstFrameInclusive, int lastFrameExclusive) const {
    std::unique_ptr<std::list<Rectangle>> rectangles(new std::list<Rectangle>());
    std::string query = "SELECT frame, x, y, width, height FROM %s WHERE ("
            + metadataSpecification_.whereClauseConstraints(false)
            + ") AND frame >= " + std::to_string(firstFrameInclusive)
            + " AND frame < " + std::to_string(lastFrameExclusive);

    selectFromMetadataAndApplyFunction(query.c_str(), [&](sqlite3_stmt *stmt) {
        unsigned int frame = sqlite3_column_int(stmt, 0);
        unsigned int x = sqlite3_column_int(stmt, 1);
        unsigned int y = sqlite3_column_int(stmt, 2);
        unsigned int width = sqlite3_column_int(stmt, 3);
        unsigned int height = sqlite3_column_int(stmt, 4);

        rectangles->emplace_back(frame, x, y, width, height);
    });
    return rectangles;
}

std::unique_ptr<std::list<Rectangle>> MetadataManager::rectanglesForAllObjectsForFrames(int firstFrameInclusive, int lastFrameExclusive) const {
    std::unique_ptr<std::list<Rectangle>> rectangles(new std::list<Rectangle>());
    std::string query = "SELECT frame, x, y, width, height FROM " + metadataSpecification_.tableName() + " where frame >= " + std::to_string(firstFrameInclusive) + " and frame < " + std::to_string(lastFrameExclusive) + ";";
    selectFromMetadataWithoutQuerySubstitutionsAndApplyFunction(query.c_str(), query.length(), [&](sqlite3_stmt *stmt) {
        unsigned int frame = sqlite3_column_int(stmt, 0);
        unsigned int x = sqlite3_column_int(stmt, 1);
        unsigned int y = sqlite3_column_int(stmt, 2);
        unsigned int width = sqlite3_column_int(stmt, 3);
        unsigned int height = sqlite3_column_int(stmt, 4);

        rectangles->emplace_back(frame, x, y, width, height);
    });
    return rectangles;
}

static void doesRectangleIntersectTileRectangle(sqlite3_context *context, int argc, sqlite3_value **argv) {
    assert(argc == 4);

    Rectangle *tileRectangle = reinterpret_cast<Rectangle*>(sqlite3_user_data(context));

    unsigned int x = sqlite3_value_int(argv[0]);
    unsigned int y = sqlite3_value_int(argv[1]);
    unsigned int width = sqlite3_value_int(argv[2]);
    unsigned int height = sqlite3_value_int(argv[3]);

    if (tileRectangle->intersects(Rectangle(0, x, y, width, height)))
        sqlite3_result_int(context, 1);
    else
        sqlite3_result_int(context, 0);
}

std::unordered_set<int> MetadataManager::idealKeyframesForMetadataAndTiles(const tiles::TileLayout &tileLayout) const {
    auto numberOfTiles = tileLayout.numberOfTiles();
    std::vector<std::vector<int>> framesForEachTile(numberOfTiles);
    for (auto i = 0u; i < numberOfTiles; ++i)
        framesForEachTile[i] = framesForTileAndMetadata(i, tileLayout);

    std::vector<int> globalFrames = orderedFramesForMetadata();
    if (!globalFrames.size())
        return {};

    enum class Inclusion {
        InGlobalFrames,
        NotInGlobalFrames,
    };

    std::vector<std::vector<int>::const_iterator> tileFrameIterators(numberOfTiles);
    std::transform(framesForEachTile.begin(), framesForEachTile.end(), tileFrameIterators.begin(), [](const auto &frames) {
        return frames.begin();
    });
    std::vector<int>::const_iterator globalFramesIterator = globalFrames.begin();

    std::vector<Inclusion> tileInclusions(numberOfTiles, Inclusion::NotInGlobalFrames);
    auto updateTileInclusions = [&]() -> bool {
        bool anyChanged = false;
        for (auto tileIndex = 0u; tileIndex < numberOfTiles; ++tileIndex) {
            if (tileFrameIterators[tileIndex] != framesForEachTile[tileIndex].end()) {
                if (*tileFrameIterators[tileIndex] == *globalFramesIterator) {
                    if (tileInclusions[tileIndex] == Inclusion::NotInGlobalFrames)
                        anyChanged = true;
                    tileInclusions[tileIndex] = Inclusion::InGlobalFrames;
                    ++tileFrameIterators[tileIndex];
                } else {
                    if (tileInclusions[tileIndex] == Inclusion::InGlobalFrames)
                        anyChanged = true;
                    tileInclusions[tileIndex] = Inclusion::NotInGlobalFrames;
                }
            } else {
                if (tileInclusions[tileIndex] == Inclusion::InGlobalFrames)
                    anyChanged = true;
                tileInclusions[tileIndex] = Inclusion::NotInGlobalFrames;
            }
        }
        return anyChanged;
    };
    // Set initial state of tile inclusions. We don't care about whether any change because the first frame is always a keyframe.
    updateTileInclusions();

    // Add the first global frame to the set of keyframes.
    std::unordered_set<int> keyframes;
    keyframes.insert(*globalFramesIterator++);

    for(; globalFramesIterator != globalFrames.end(); globalFramesIterator++) {
        if (updateTileInclusions())
            keyframes.insert(*globalFramesIterator);
    }

    // Insert frames from normal sequence.
    auto &originalIdealKeyframes = idealKeyframesForMetadata();
    keyframes.insert(originalIdealKeyframes.begin(), originalIdealKeyframes.end());

    return keyframes;
}


std::vector<int> MetadataManager::framesForTileAndMetadata(unsigned int tile, const tiles::TileLayout &tileLayout) const {
    // Get rectangle for tile.
    auto tileRectangle = tileLayout.rectangleForTile(tile);

    // Select frames that have a rectangle that intersects the tile rectangle.
    auto registerIntersectionFunction = [&](sqlite3 *db) {
        ASSERT_SQLITE_OK(sqlite3_create_function(db, "RECTANGLEINTERSECT", 4, SQLITE_UTF8 | SQLITE_DETERMINISTIC, &tileRectangle, doesRectangleIntersectTileRectangle, NULL, NULL));
    };

    std::string query = "SELECT DISTINCT frame FROM %s WHERE " + metadataSpecification_.whereClauseConstraints(true) + " AND RECTANGLEINTERSECT(x, y, width, height) == 1;";
    std::vector<int> frames;
    selectFromMetadataAndApplyFunctionWithFrameLimits(query.c_str(), [&](sqlite3_stmt *stmt) {
        int queryFrame = sqlite3_column_int(stmt, 0);
        frames.push_back(queryFrame);
    }, registerIntersectionFunction);

    return frames;
}





} // namespace lightdb::metadata
