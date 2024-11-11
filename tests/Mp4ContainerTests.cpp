#include "TestContainer.h"

#include <vd/Mp4Container.h>

#include <gtest/gtest.h>

using namespace vd;

namespace
{

void RemoveChild(AP4_AtomParent &atom, const char *path)
{
    auto child = atom.FindChild(path);
    child->GetParent()->RemoveChild(child);
}



TEST(Mp4ContainerTests, ConstructionNullStream)
{
    ASSERT_THROW(Mp4Container{nullptr}, ArgumentError);
}

TEST(Mp4ContainerTests, BadFormat)
{
    ASSERT_THROW(Mp4Container{std::make_shared<AP4_MemoryByteStream>()}, NotFoundError);

    auto data = GetTestData();
    data.ftypAtom->SetMajorBrandAndVersion(0x77777777, 0);
    ASSERT_THROW(Mp4Container{Serialize(data)}, NotSupportedError);

    data = GetTestData();
    auto mvhdAtom = dynamic_cast<AP4_MvhdAtom *>(data.moovAtom->FindChild("mvhd"));
    mvhdAtom->SetRate(0);
    ASSERT_THROW(Mp4Container{Serialize(data)}, NotSupportedError);
    data.moovAtom->RemoveChild(mvhdAtom);
    ASSERT_THROW(Mp4Container{Serialize(data)}, NotFoundError);

    data = GetTestData();
    RemoveChild(*data.moovAtom, "trak");
    ASSERT_THROW(Mp4Container{Serialize(data)}, NotFoundError);
    data.moovAtom->AddChild(CreateTrakAtom());
    data.moovAtom->AddChild(CreateTrakAtom());
    ASSERT_THROW(Mp4Container{Serialize(data)}, NotSupportedError);

    data = GetTestData();
    RemoveChild(*data.moovAtom, "trak/tkhd");
    ASSERT_THROW(Mp4Container{Serialize(data)}, NotFoundError);

    data = GetTestData();
    RemoveChild(*data.moovAtom, "trak/mdia/minf/stbl/stsd/avc1/avcC");
    ASSERT_THROW(Mp4Container{Serialize(data)}, NotFoundError);

    data = GetTestData();
    auto avccAtom = dynamic_cast<AP4_AvccAtom *>(data.moovAtom->FindChild("trak/mdia/minf/stbl/stsd/avc1/avcC"));
    avccAtom->SetProfile(0);
    ASSERT_THROW(Mp4Container{Serialize(data)}, NotSupportedError);
    
    data = GetTestData();
    data.sidxAtom->SetReferenceCount(0);
    ASSERT_THROW(Mp4Container{Serialize(data)}, Error);

    data = GetTestData();
    data.sidxAtom->SetReferenceId(0);
    ASSERT_THROW(Mp4Container{Serialize(data)}, Error);

    data = GetTestData();
    data.sidxAtom->SetTimeScale(0);
    ASSERT_THROW(Mp4Container{Serialize(data)}, ArgumentError);

    data = GetTestData();
    data.sidxAtom->UseReferences()[0].m_ReferencedSize = 0;
    ASSERT_THROW(Mp4Container{Serialize(data)}, ArgumentError);

    data = GetTestData();
    data.sidxAtom->UseReferences()[0].m_SubsegmentDuration = 0;
    ASSERT_THROW(Mp4Container{Serialize(data)}, ArgumentError);
}

}//unnamed namespace