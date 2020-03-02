#pragma once

namespace video
{
	struct VideoInfo
	{

		// AVFormatContext

		std::string filename; // �ļ���
		uint64_t duration;	  // ����ʱ�� ΢��
		double bitRate;		  // ƽ��������� b/s
		uint32_t nb_streams;  // ����Ƶ����

		// AVInputFormat

		std::string iformatName;        // ��װ��ʽ����
		std::string iformatLongName;	// ��װ��ʽ������
		std::string iformatExtensions;	// ��װ��ʽ��չ��
		int iformatID;					// ��װ��ʽID

		// AVStream

		uint32_t nFramesV;	   //
		uint32_t nFramesA;	   //
		uint64_t durationV;      // ��Ƶ������ ΢��
		uint64_t durationA;      // ��Ƶ������ ΢��
		double frameRateV;     // ��Ƶfps
		double frameRateA;     // ��Ƶfps
		double timeBaseV;      //
		double timeBaseA;      //

		// AVCodecParameters

		uint32_t width;	      // ��Ƶ���
		uint32_t height;      // ��Ƶ�߶�
		uint32_t bitRateV;    // ��Ƶ���� b/s

		uint32_t bitRateA;    // ��Ƶ���� b/s
		uint32_t sampleRateA; // ��Ƶ������
		uint32_t channelsA;   // ��Ƶͨ����

		// AVCodec

		std::string codecNameV;      // ��Ƶ�����ʽ����
		std::string codecLongNameV;	 // ��Ƶ�����ʽ������

	};
}
