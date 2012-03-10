#include "stdafx.h"

#include "sound_out.h"

//#define HAVE_KS_HEADERS

#define STRICT
#include <stdint.h>
#include <windows.h>
#include <XAudio2.h>
#include <assert.h>
#ifdef HAVE_KS_HEADERS
#include <ks.h>
#include <ksmedia.h>
#endif

#pragma comment ( lib, "winmm.lib" )

class sound_out_i_xaudio2 : public sound_out
{
	class XAudio2_BufferNotify : public IXAudio2VoiceCallback
	{
	public:
		HANDLE hBufferEndEvent;

		XAudio2_BufferNotify() {
			hBufferEndEvent = NULL;
			hBufferEndEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
			assert( hBufferEndEvent != NULL );
		}

		~XAudio2_BufferNotify() {
			CloseHandle( hBufferEndEvent );
			hBufferEndEvent = NULL;
		}

		STDMETHOD_( void, OnBufferEnd ) ( void *pBufferContext ) {
			assert( hBufferEndEvent != NULL );
			SetEvent( hBufferEndEvent );
			sound_out_i_xaudio2 * psnd = ( sound_out_i_xaudio2 * ) pBufferContext;
			if ( psnd ) psnd->OnBufferEnd();
		}


		// dummies:
		STDMETHOD_( void, OnVoiceProcessingPassStart ) ( UINT32 BytesRequired ) {}
		STDMETHOD_( void, OnVoiceProcessingPassEnd ) () {}
		STDMETHOD_( void, OnStreamEnd ) () {}
		STDMETHOD_( void, OnBufferStart ) ( void *pBufferContext ) {}
		STDMETHOD_( void, OnLoopEnd ) ( void *pBufferContext ) {}
		STDMETHOD_( void, OnVoiceError ) ( void *pBufferContext, HRESULT Error ) {};
	};

	void OnBufferEnd()
	{
		InterlockedDecrement( &buffered_count );
	}

	void          * hwnd;
	bool            paused;
	unsigned        reopen_count;
	unsigned        sample_rate, nch, max_samples_per_frame, num_frames;
	volatile LONG   buffered_count;
	LONG            buffer_write_cursor;

	int16_t       * sample_buffer;

	IXAudio2               *xaud;
	IXAudio2MasteringVoice *mVoice; // listener
	IXAudio2SourceVoice    *sVoice; // sound source
	XAUDIO2_BUFFER         *buf;
	XAUDIO2_VOICE_STATE     vState;
	XAudio2_BufferNotify    notify; // buffer end notification
public:
	sound_out_i_xaudio2()
	{
		paused = false;
		reopen_count = 0;
		buffered_count = 0;

		xaud = NULL;
		mVoice = NULL;
		sVoice = NULL;
		sample_buffer = NULL;
		ZeroMemory( &vState, sizeof( vState ) );
	}

	virtual ~sound_out_i_xaudio2()
	{
		close();
	}

	virtual const char* open( void * hwnd, unsigned sample_rate, unsigned nch, unsigned max_samples_per_frame, unsigned num_frames )
	{
		this->hwnd = hwnd;
		this->sample_rate = sample_rate;
		this->nch = nch;
		this->max_samples_per_frame = max_samples_per_frame;
		this->num_frames = num_frames;

#ifdef HAVE_KS_HEADERS
		WAVEFORMATEXTENSIBLE wfx;
		wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		wfx.Format.nChannels = nch; //1;
		wfx.Format.nSamplesPerSec = sample_rate;
		wfx.Format.nBlockAlign = 2 * nch; //2;
		wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
		wfx.Format.wBitsPerSample = 16;
		wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);
		wfx.Samples.wValidBitsPerSample = 16;
		wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
		wfx.dwChannelMask = nch == 2 ? KSAUDIO_SPEAKER_STEREO : KSAUDIO_SPEAKER_MONO;
#else
		WAVEFORMATEX wfx;
		wfx.wFormatTag = WAVE_FORMAT_PCM;
		wfx.nChannels = nch; //1;
		wfx.nSamplesPerSec = sample_rate;
		wfx.nBlockAlign = 2 * nch; //2;
		wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
		wfx.wBitsPerSample = 16;
		wfx.cbSize = 0;
#endif
        HRESULT hr = XAudio2Create( &xaud, 0 );
		if (FAILED(hr)) return "Creating XAudio2 interface";
		hr = xaud->CreateMasteringVoice(
			&mVoice,
			nch,
			sample_rate,
			0,
			NULL,
			NULL );
		if (FAILED(hr)) return "Creating XAudio2 mastering voice";
		hr = xaud->CreateSourceVoice( &sVoice, &wfx, 0, 4.0f, &notify );
		if (FAILED(hr)) return "Creating XAudio2 source voice";
		hr = sVoice->Start( 0 );
		if (FAILED(hr)) return "Starting XAudio2 voice";
		hr = sVoice->SetFrequencyRatio((float)1.0f);
		if (FAILED(hr)) return "Setting XAudio2 voice frequency ratio";
		buf = new XAUDIO2_BUFFER[ num_frames ];
		memset( buf, 0, sizeof( XAUDIO2_BUFFER ) * num_frames );
		buffered_count = 0;
		buffer_write_cursor = 0;
		sample_buffer = new int16_t[ max_samples_per_frame * num_frames ];
		return NULL;
	}

	void close()
	{
		if( sVoice ) {
			if( !paused ) {
				sVoice->Stop( 0 );
			}
			sVoice->DestroyVoice();
		}

		if( mVoice ) {
			mVoice->DestroyVoice();
		}

		if( xaud ) {
			xaud->Release();
			xaud = NULL;
		}

		delete [] sample_buffer;
		sample_buffer = NULL;
	}

	virtual const char* write_frame( void * buffer, unsigned num_samples, bool wait )
	{
		if ( paused )
		{
			if ( wait ) Sleep( MulDiv( num_samples / nch, 1000, sample_rate ) );
			return 0;
		}

		if ( reopen_count )
		{
			if ( ! --reopen_count )
			{
				const char * err = open( hwnd, sample_rate, nch, max_samples_per_frame, num_frames );
				if ( err )
				{
					reopen_count = 60 * 5;
					return err;
				}
			}
			else
			{
				if ( wait ) Sleep( MulDiv( num_samples / nch, 1000, sample_rate ) );
				return 0;
			}
		}

		for (;;) {
			sVoice->GetState( &vState );
			assert( vState.BuffersQueued <= num_frames );
			if( vState.BuffersQueued < num_frames ) {
				if( vState.BuffersQueued == 0 ) {
					// buffers ran dry
				}
				// there is at least one free buffer
				break;
			} else {
				// wait for one buffer to finish playing
				ResetEvent( notify.hBufferEndEvent );
				WaitForSingleObject( notify.hBufferEndEvent, INFINITE );
			}
		}
		XAUDIO2_BUFFER buf = {0};
		unsigned num_bytes = num_samples * 2;
		buf.AudioBytes = num_bytes;
		buf.pAudioData = ( const BYTE * )( sample_buffer + max_samples_per_frame * buffer_write_cursor );
		buffer_write_cursor = ( buffer_write_cursor + 1 ) % num_frames;
		memcpy( ( void * ) buf.pAudioData, buffer, num_bytes );
		if( sVoice->SubmitSourceBuffer( &buf ) == S_OK )
		{
			InterlockedIncrement( &buffered_count );
			return 0;
		}

		close();
		reopen_count = 60 * 5;

		return 0;
	}

	virtual const char* pause( bool pausing )
	{
		if ( pausing )
		{
			if ( ! paused )
			{
				paused = true;
				HRESULT hr = sVoice->Stop( 0 );
				if ( FAILED(hr) )
				{
					close();
					reopen_count = 60 * 5;
				}
			}
		}
		else
		{
			if ( paused )
			{
				paused = false;
				HRESULT hr = sVoice->Start( 0 );
				if ( FAILED(hr) )
				{
					close();
					reopen_count = 60 * 5;
				}
			}
		}

		return 0;
	}

	virtual unsigned buffered()
	{
		return buffered_count;
	}

};

sound_out * create_sound_out_xaudio2()
{
	return new sound_out_i_xaudio2;
}
