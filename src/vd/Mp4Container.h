#ifndef VDOWNLOADER_VD_MP4_CONTAINER_H_
#define VDOWNLOADER_VD_MP4_CONTAINER_H_

#include "Mp4Utils.h"
#include "Track.h"

#include <cstdint>
#include <optional>

namespace vd
{

class Mp4Container final
{
public:
    explicit Mp4Container(std::shared_ptr<AP4_ByteStream> data);
    
    const Track &GetTrack() const noexcept;
    Track &GetTrack() noexcept;

private:
    //optional is just for late initialization
    std::optional<Track> mTrack;
};

} //namespace vd

#endif //VDOWNLOADER_VD_MP4_CONTAINER_H_