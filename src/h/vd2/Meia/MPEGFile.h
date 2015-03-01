#ifndef f_VD2_MEIA_MPEGFILE_H
#define f_VD2_MEIA_MPEGFILE_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/refcount.h>

struct VDMPEGSequenceHeader {
	int		width;
	int		height;
	int		aspect;
	int		framerate;
	int		bitrate;
	int		vbvbuffersize;
	bool	constrained:1,
			custom_intra:1,
			custom_nonintra:1;

	unsigned char	intra[64];
	unsigned char	nonintra[64];
};

struct VDMPEGAudioFormat {
	int		sampling_rate;
	int		channels;
};

class VDINTERFACE IVDMPEGVideoStream : public IVDRefCount {
public:
	enum FrameType {
		kIFrame = 1,
		kPFrame = 2,
		kBFrame = 3,
		kDFrame	= 4
	};

	virtual void getInitialFormat(VDMPEGSequenceHeader&)=0;
	virtual long getFrameCount()=0;
	virtual long getNearestI(long f)=0;
	virtual long getPrevIP(long f)=0;
	virtual long getNextIP(long f)=0;
	virtual long DisplayToStreamOrder(long f)=0;
	virtual long StreamToDisplayOrder(long f)=0;
	virtual long ReadPicture(long display_number, void *buf, long maxbuffer, FrameType& type)=0;
};

class VDINTERFACE IVDMPEGAudioStream : public IVDRefCount {
public:
	virtual void getInitialFormat(VDMPEGAudioFormat& format)=0;
	virtual unsigned long getFrameCount()=0;
	virtual long ReadFrames(long frame_num, int frame_count, void *buf, long maxbuffer, long& bytes_read)=0;
};

class VDINTERFACE IVDMPEGFile : public IVDRefCount {
public:
	virtual void Open(const wchar_t *pszFilename)=0;
	virtual void Close()=0;
	virtual bool Init()=0;
	virtual long getAudioStreamMask()=0;
	virtual long getVideoStreamMask()=0;
	virtual IVDMPEGAudioStream *getAudioStream(int n)=0;
	virtual IVDMPEGVideoStream *getVideoStream(int n)=0;
	virtual bool PreScan()=0;
};

IVDMPEGFile *CreateVDMPEGFile();

#endif
