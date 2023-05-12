#include "CZIreadAPI.h"

using namespace libCZI;
using namespace std;


CZIreadAPI::CZIreadAPI(const std::wstring& fileName) {

	auto stream = libCZI::CreateStreamFromFile(fileName.c_str());
	auto spReader = libCZI::CreateCZIReader();
	spReader->Open(stream);
	
	this->spAccessor = spReader->CreateSingleChannelScalingTileAccessor();
	this->spReader = spReader;
}

std::string CZIreadAPI::GetXmlMetadata() {

	auto mds = this->spReader->ReadMetadataSegment();
	auto md = mds->CreateMetaFromMetadataSegment();

	return md->GetXml();
}

size_t CZIreadAPI::GetDimensionSize(libCZI::DimensionIndex DimIndex) {

	auto stats = this->spReader->GetStatistics();
	int size;

	// Should replace nullptr with reference to handle CZI with index not starting at 0, legal ?
	bool DimExist = stats.dimBounds.TryGetInterval(DimIndex, nullptr, &size);

	if (DimExist) {
		return size;
	}
	
	return 0;
}

libCZI::PixelType CZIreadAPI::GetChannelPixelType(int chanelIdx) {

	libCZI::SubBlockInfo sbBlkInfo;

	bool b = this->spReader->TryGetSubBlockInfoOfArbitrarySubBlockInChannel(chanelIdx, sbBlkInfo);
	if (!b) {
		// TODO more precise error handling
		return libCZI::PixelType::Invalid;
	}

	return sbBlkInfo.pixelType;
}


libCZI::SubBlockStatistics CZIreadAPI::GetSubBlockStats() {
	
	return this->spReader->GetStatistics();
}

std::unique_ptr<PImage> CZIreadAPI::GetSingleChannelScalingTileAccessorData(libCZI::PixelType pixeltype, libCZI::IntRect roi, libCZI::RgbFloatColor bgColor, float zoom, const std::string& coordinateString, const std::wstring& SceneIndexes) {


	libCZI::CDimCoordinate planeCoordinate;
	try
	{
		planeCoordinate = CDimCoordinate::Parse(coordinateString.c_str());
	}
	catch (libCZI::LibCZIStringParseException& parseExcp)
	{
		//TODO Error handling
	}

	libCZI::ISingleChannelScalingTileAccessor::Options scstaOptions; scstaOptions.Clear();
	scstaOptions.backGroundColor = bgColor;
	if (!SceneIndexes.empty()) {
		scstaOptions.sceneFilter = libCZI::Utils::IndexSetFromString(SceneIndexes);
	}
	
	std::shared_ptr<libCZI::IBitmapData> Data = this->spAccessor->Get(pixeltype, roi, &planeCoordinate, zoom, &scstaOptions);
	std::unique_ptr<PImage> ptr_Bitmap(new PImage(Data));

	return ptr_Bitmap;
}

std::unique_ptr<PImage> CZIreadAPI::GetComposeMultiChannelData(libCZI::RgbFloatColor bgColor, libCZI::IntRect roi, float zoom){
	auto statistics = this->spReader->GetStatistics();

	// get the display-setting from the document's metadata
	auto mds = this->spReader->ReadMetadataSegment();
	auto md = mds->CreateMetaFromMetadataSegment();
	auto docInfo = md->GetDocumentInfo();
	auto dsplSettings = docInfo->GetDisplaySettings();

	// get the tile-composite for all channels (which are marked 'active' in the display-settings)
	std::vector<shared_ptr<libCZI::IBitmapData>> actvChBms;
	int index = 0;  // index counting only the active channels
	std::map<int, int> activeChNoToChIdx;   // we need to keep track which 'active channels" corresponds to which channel index

	libCZI::ISingleChannelScalingTileAccessor::Options scstaOptions;
	scstaOptions.Clear();
	scstaOptions.backGroundColor = bgColor;
	scstaOptions.sortByM = true;

	libCZI::CDisplaySettingsHelper::EnumEnabledChannels(dsplSettings.get(),
		[&](int chIdx)->bool
	{	
		libCZI::CDimCoordinate planeCoord{ { libCZI::DimensionIndex::C, chIdx } };
		actvChBms.emplace_back(this->spAccessor->Get(roi, &planeCoord, zoom, &scstaOptions));
		activeChNoToChIdx[chIdx] = index++;
		return true;
	});

	// initialize the helper with the display-settings and provide the pixeltypes 
	// (for each active channel)
	libCZI::CDisplaySettingsHelper dsplHlp;
	dsplHlp.Initialize(dsplSettings.get(), 
		[&](int chIdx)->libCZI::PixelType { return actvChBms[activeChNoToChIdx[chIdx]]->GetPixelType(); });
	
	// pass the tile-composites we just created (and the display-settings for the those active 
	//  channels) into the multi-channel-composor-function
	auto mcComposite = libCZI::Compositors::ComposeMultiChannel_Bgr24(
		dsplHlp.GetActiveChannelsCount(),
		std::begin(actvChBms),
		dsplHlp.GetChannelInfosArray());

	std::unique_ptr<PImage> ptr_TintedImage(new PImage(mcComposite));
	return ptr_TintedImage;
}

std::pair<void*,size_t> CZIreadAPI::GetAttachmentRawData(int index){
	const void* ptrData; size_t size;
	std::pair<void*,size_t> rawData;

	std::shared_ptr<libCZI::IAttachment> attchmnt = this->spReader->ReadAttachment(index);
	attchmnt->DangerousGetRawData(ptrData,size);

	libCZI::AttachmentInfo attchmntInfo = attchmnt->GetAttachmentInfo();
	std::string attchmntName = attchmntInfo.name;
	// std::string filename = attchmntName+".file";
	// std::ofstream save_rawData(filename, ios::out|ios::binary);

	void* buf = malloc(size);
	memcpy(buf,ptrData,size);
	rawData.first = buf;
	rawData.second = size;
	return rawData;
}

std::vector<std::string> CZIreadAPI::GetAttachmentNames(){
	int attchmentSize = this->spReader->GetAttachmentLength();
	
	std::vector<std::string> attchmntNames;
	for (int i=0;i<attchmentSize;i++){
		std::shared_ptr<libCZI::IAttachment> attchmnt = this->spReader->ReadAttachment(i);
		//attchmnt->DangerousGetRawData(ptrData,size);
		libCZI::AttachmentInfo attchmntInfo = attchmnt->GetAttachmentInfo();
		attchmntNames.emplace_back(attchmntInfo.name);
	}
	return attchmntNames;
}

std::unique_ptr<PImage> CZIreadAPI::GetLabelImage(int index){
	// only used for extracting the label image
	const void* ptrData; size_t size;
	std::pair<void*,size_t> rawData;
	std::shared_ptr<libCZI::IAttachment> attchmnt = this->spReader->ReadAttachment(index);
	libCZI::IAttachment* attchmnt_cpy = attchmnt.get();

	auto stream = libCZI::CreateStreamFromMemory(attchmnt_cpy);
	auto attchReader = libCZI::CreateCZIReader();
	attchReader->Open(stream);
	
	auto attchAccessor = attchReader->CreateSingleChannelScalingTileAccessor();
	auto attchStatistics = attchReader->GetStatistics();

	libCZI:CDimCoordinate attchPlane{{libCZI::DimensionIndex::C, 0}};
	auto attchBitmap = attchAccessor->Get(
		libCZI::IntRect{0,0,attchStatistics.boundingBox.w,attchStatistics.boundingBox.h},
		&attchPlane,
		1,
		nullptr);
	std::unique_ptr<PImage> attchImage(new PImage(attchBitmap));
	return attchImage;
} 