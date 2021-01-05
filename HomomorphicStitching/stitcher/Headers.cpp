//
// Created by sophi on 4/10/2018.
//

#include <vector>
#include <string>
#include <cassert>
#include <iostream>
#include "Headers.h"
#include "Nal.h"
#include "SequenceParameterSet.h"
#include "VideoParameterSet.h"
#include "PictureParameterSet.h"

using std::vector;

namespace lightdb {

	Headers::Headers(const tiles::Context &context, vector<bytestring> nals) {

		for (int i = 0; i < kNumHeaders; i++) {
	  		Nal *current_nal = Load(context, nals[i]);
			assert(current_nal->IsHeader());
            headers_.push_back(current_nal);
            if (current_nal->IsSequence()) {
                sequence_ = reinterpret_cast<SequenceParameterSet*>(current_nal);
            } else if (current_nal->IsPicture()) {
                picture_ = reinterpret_cast<PictureParameterSet*>(current_nal);
            } else if (current_nal->IsVideo()) {
                video_ = reinterpret_cast<VideoParameterSet*>(current_nal);
            }
		}

		assert(headers_.size() == kNumHeaders);
	}

	bytestring Headers::GetBytes() const {
		bytestring bytes;
	    for (auto header : headers_) {
	    	auto header_bytes = header->GetBytes();
	    	bytes.insert(bytes.end(), header_bytes.begin(), header_bytes.end());
	    }
	    return bytes;
	}

    PictureParameterSet* Headers::GetPicture() const {
	    return picture_;
	}

	SequenceParameterSet* Headers::GetSequence() const {
	    return sequence_;
	}

    VideoParameterSet* Headers::GetVideo() const {
	    return video_;
	}

}
