#ifndef VDOWNLOADER_TESTS_TEST_CONTAINER_H_
#define VDOWNLOADER_TESTS_TEST_CONTAINER_H_

#include <vd/Ap4Headers.h>
#include <vd/Ap4Helpers.h>
#include <vd/Utils.h>

#include <algorithm>
#include <memory>
#include <vector>

using namespace vd::literals;

namespace {
    
const std::byte gSeqParams[] = {1_b, 2_b, 3_b, 4_b};
const std::byte gPicParams[] = {4_b, 3_b, 2_b, 1_b};



struct TestContainerData final
{
    std::unique_ptr<AP4_FtypAtom> ftypAtom;
    std::unique_ptr<AP4_MoovAtom> moovAtom;
    std::unique_ptr<AP4_SidxAtom> sidxAtom;
};

struct FakeSampleTable final : public AP4_SampleTable
{
    AP4_Cardinal GetSampleCount() override;
    AP4_Result GetSample(AP4_Ordinal, AP4_Sample &) override;
    AP4_Result GetSampleChunkPosition(AP4_Ordinal,
                                      AP4_Ordinal &,
                                      AP4_Ordinal &) override;
    AP4_Cardinal GetSampleDescriptionCount() override;
    AP4_SampleDescription *GetSampleDescription(AP4_Ordinal) override;
    AP4_Result GetSampleIndexForTimeStamp(AP4_UI64, AP4_Ordinal &) override;
    AP4_Ordinal GetNearestSyncSampleIndex(AP4_Ordinal, bool) override;
};

AP4_Cardinal FakeSampleTable::GetSampleCount()
{
    return 0;
}

AP4_Result FakeSampleTable::GetSample(AP4_Ordinal, AP4_Sample &)
{
    return AP4_ERROR_NOT_SUPPORTED;
}

AP4_Result FakeSampleTable::GetSampleChunkPosition(AP4_Ordinal,
                                                   AP4_Ordinal &,
                                                   AP4_Ordinal &)
{
    return AP4_ERROR_NOT_SUPPORTED;
}

AP4_Cardinal FakeSampleTable::GetSampleDescriptionCount()
{
    return 1;
}

AP4_SampleDescription *FakeSampleTable::GetSampleDescription(AP4_Ordinal)
{
    AP4_Array<AP4_DataBuffer> seqParams;
    seqParams.Append({gSeqParams, sizeof(gSeqParams)});
    AP4_Array<AP4_DataBuffer> picParams;
    picParams.Append({gPicParams, sizeof(gPicParams)});

    return
        new AP4_AvcSampleDescription(
            AP4_ATOM_TYPE('a', 'v', 'c', '1'),
            0,
            0,
            0,
            "",
            new AP4_AvccAtom{
                'M',
                0,
                0,
                2,
                0,
                0,
                0,
                seqParams,
                picParams});
}

AP4_Result FakeSampleTable::GetSampleIndexForTimeStamp(AP4_UI64, AP4_Ordinal &)
{
    return AP4_ERROR_NOT_SUPPORTED;
}

AP4_Ordinal FakeSampleTable::GetNearestSyncSampleIndex(AP4_Ordinal, bool)
{
    return 0;
}



AP4_TrakAtom *CreateTrakAtom()
{
    return new AP4_TrakAtom{new FakeSampleTable{}, 0, "", 111, 0, 0, 0, 0, 0, 0, "", 0, 0};
}

std::unique_ptr<AP4_SidxAtom> CreateSidxAtom()
{
    auto sidxAtom = std::make_unique<AP4_SidxAtom>(111, 1, 0, 0);

    //This line has to be placed before appending refs, otherwise it will mess up size.
    //Actually, you are not allowed to .Append, it will mess up size again,
    //you have to use [] operator
    sidxAtom->SetReferenceCount(3);

    sidxAtom->UseReferences()[0].m_ReferencedSize = 1;
    sidxAtom->UseReferences()[0].m_SubsegmentDuration = 10;

    sidxAtom->UseReferences()[1].m_ReferencedSize = 2;
    sidxAtom->UseReferences()[1].m_SubsegmentDuration = 20;

    sidxAtom->UseReferences()[2].m_ReferencedSize = 3;
    sidxAtom->UseReferences()[2].m_SubsegmentDuration = 30;

    return sidxAtom;
}

TestContainerData GetTestData()
{
    auto ftypAtom = std::make_unique<AP4_FtypAtom>(AP4_ATOM_TYPE('d', 'a', 's', 'h'), 0);

    auto moovAtom = std::make_unique<AP4_MoovAtom>();
    moovAtom->AddChild(new AP4_MvhdAtom{0, 0, 0, 0, 0x10000, 0});
    moovAtom->AddChild(CreateTrakAtom());

    return TestContainerData{.ftypAtom = std::move(ftypAtom),
        .moovAtom = std::move(moovAtom),
        .sidxAtom = CreateSidxAtom()};
}

std::shared_ptr<AP4_MemoryByteStream> Serialize(TestContainerData &data)
{
    auto stream = std::make_shared<AP4_MemoryByteStream>();

    if(data.ftypAtom != nullptr)
    {
        data.ftypAtom->Write(*stream);
    }
    if(data.moovAtom != nullptr)
    {
        data.moovAtom->Write(*stream);
    }
    if(data.sidxAtom != nullptr)
    {
        data.sidxAtom->Write(*stream);

        AP4_UI32 payloadSize{0};
        for(const auto &ref : data.sidxAtom->GetReferences())
        {
            payloadSize += ref.m_ReferencedSize;
        }
        std::vector<std::byte> buf(vd::UintCast<std::size_t>(payloadSize), 0xAB_b);
        stream->Write(buf.data(), vd::UintCast<AP4_Size>(buf.size()));
    }

    stream->Seek(0);
    return stream;
}

std::shared_ptr<AP4_MemoryByteStream> Serialize(TestContainerData &&data)
{
    return Serialize(data);
}

}//unnamed namespace

#endif //VDOWNLOADER_TESTS_TEST_CONTAINER_H_