// based on https://community.atmel.com/projects/sd-card-midi-player

#include <stdio.h>
#include <unistd.h>

// Controller
#define MF_Bank_Select_MSB    0x00	// 0x00 .Bank Select MSB ( value 0x50 : Preset A Patch 1..128, 0x51 Preset B Patch 129..255 )
#define MF_Bank_Select_LSB    0x20	// 0x20 .Bank Select LSB ( value 0x00 )
#define MF_Modulation         0x01	// 0x01 .Modulation
#define MF_Breath             0x02	// 0x02  Breath Controller
#define MF_Foot               0x04	// 0x04  Foot Controller
#define MF_Portamento_Time    0x05	// 0x05 .Portamento Time
#define MF_Main_Volume        0x07	// 0x07 .Main Volume
#define MF_Balance            0x08	// 0x08  Balance
#define MF_Pan                0x0A	// 0x0A .Pan
#define MF_Expression         0x0B	// 0x0B .Expression Controller
#define MF_Effect_1           0x0C	// 0x0C  Effect Control 1
#define MF_Effect_2           0x0D	// 0x0D  Effect Control 2
#define MF_General_1to4       0x13	// 0x13  General-Purpose Controllers 1-4
#define MF_Controller_LSB     0x3F	// 0x3F  LSB for controllers 0-31
#define MF_Sustain            0x40	// 0x40 .Sustain( Damper pedal / Hold 1)
#define MF_Portamento         0x41	// 0x41 .Portamento
#define MF_Sostenuto          0x42	// 0x42 .Sostenuto
#define MF_Soft               0x43	// 0x43 .Soft Pedal
#define MF_Legato             0x44	// 0x44  Legato Footswitch
#define MF_Hold               0x45	// 0x45  Hold 2
#define MF_Control_1          0x46	// 0x46  Sound Controller 1 (default: Timber Variation)
#define MF_Control_2          0x47	// 0x47  Sound Controller 2 (default: Timber/Harmonic Content)
#define MF_Control_3          0x48	// 0x48  Sound Controller 3 (default: Release Time)
#define MF_Control_4          0x49	// 0x49  Sound Controller 4 (default: Attack Time)
#define MF_Portamento_Ctrl    0x54	// 0x54  Portamento Control
#define MF_Reverb             0x5B	// 0x5B .Effects 1 Depth (M-GS64 : Reverb send level)
#define MF_Effects_2          0x5C	// 0x5C  Effects 2 Depth (formerly Tremolo Depth)
#define MF_Chorus             0x5D	// 0x5D .Effects 3 Depth (M-GS64 : Chorus send level)
#define MF_Effects_4          0x5E	// 0x5E  Effects 4 Depth (formerly Celeste Detune)
#define MF_Effects_5          0x5F	// 0x5F  Effects 5 Depth (formerly Phaser Depth)
#define MF_Data_Increment     0x60	// 0x60  Data Increment
#define MF_Data_Decrement     0x61	// 0x61  Data Decrement
#define MF_NRPN_LSB           0x62	// 0x62 .Non-Registered Parameter Number (LSB)
#define MF_NRPN_MSB           0x63	// 0x63 .Non-Registered Parameter Number (MSB)
#define MF_RPN_LSB            0x64	// 0x64 .Registered Parameter Number (LSB)
#define MF_RPN_MSB            0x65	// 0x65 .Registered Parameter Number (MSB)
#define MF_Mode_Message       0x7F	// 0x7F  Mode Messages
#define MF_Data_Entry_MSB     0x06	// 0x06  Data Entry (MSB)
#define MF_Data_Entry_LSB     0x26	// 0x26  Data Entry (LSB)

// MIDI File Formats
#define MF_Single_track       0x00
#define MF_Parallel_tracks    0x01
#define MF_Sequential_tracks  0x02

// Meta Events Type
#define MF_Meta_Sequence         0x00  // Sequence number
#define MF_Meta_Text             0x01  // Text event
#define MF_Meta_Copyright        0x02  // Copyright
#define MF_Meta_Track_name       0x03  // track name
#define MF_Meta_Instrument_name  0x04  // Instrument name
#define MF_Meta_Lyric            0x05  // Lyric text
#define MF_Meta_Marker           0x06  // Marker text
#define MF_Meta_Cue_point        0x07  // Cue point
#define MF_Meta_MIDI_channel     0x20  // MIDI channel
#define MF_Meta_MIDI_Port        0x21  // MIDI Port
#define MF_Meta_Track_End        0x2F  // End of track
#define MF_Meta_Tempo            0x51  // tempo setting
#define MF_Meta_SMPTE_offset     0x54  // SMPTE offset
#define MF_Meta_Time_signature   0x58  // Time signature
#define MF_Meta_Key_signature    0x59  // Key signature
#define MF_Meta_Special          0x7F  // Seq. special

// FILE header INFORMATION
typedef struct
{
  uint8_t  chk[4];
  uint32_t length;
  uint16_t format;
  uint16_t ntracks;
  uint16_t division;
} MTHD;

// TRACK INFORMATION
typedef struct
{
  uint8_t  chk[4];
  uint32_t length;
} MTRK;

// EVENT INFORMATION
#define maxdata 128
typedef struct
{
  uint32_t wait;
  uint8_t  event;
  uint8_t  mtype; // only for Meta Events
  uint32_t nbdata;
  uint8_t  data[maxdata];
} MTEV;

// RETURN CODES
enum MIDIerrors
{
  NoError        = 0,
  badFileheader  = 1,
  badTrackheader = 2,
  badEvent       = 3,
  endOfFile      = 4,
	userStop       = 5
};

// FUNCTIONS
void     clearBuffer(void);
uint8_t  readByte(void);
//uint8_t  readTrackByte(void);
uint8_t  readHeaderChunk(void);
uint8_t  readTrackChunk(void);
uint16_t read16(void);
uint32_t read32(void);
uint32_t readVariableLength(void);
uint8_t  readNdata(uint8_t start);
uint8_t  readTrackEvent(void);
void     allSoundOff(void);

// Position in track
uint32_t tpos     = 0;
uint32_t prevpos  = 0;

// LAST EVENT READ
uint8_t runningEvent = 0;

// TEMPO (microsec/beat)
uint32_t tempo = 500000;

// Shared variables
MTHD midiheader;
MTRK miditrack;
MTEV midievent;

uint32_t millis = 0;
uint32_t nextTime = 0;

uint8_t sdDatIdx = 255;
uint8_t* sdData = (uint8_t*)0x8200;

// length is tracked by midi reader so we don't need to do it here
//
uint8_t SDgetc(void) __naked
{
    #asm
    ld    hl,$8200

    ld    a,(_sdDatIdx)
    inc   a
    ld    (_sdDatIdx),a
    jr    nz,getdata

    ld    (16446),a   ; 256 bytes to load

    ld    a,14        ; 00001110  - read, wait and store
    ld    (16444),a
    ld    (16447),hl
    call  $1ff4

    xor   a

getdata:
    ld    h,$82
    ld    l,a
    ld    l,(hl)
    ld    h,0
    ret
    #endasm
}


// 16444 = pr_buff

void cvtcmd(unsigned char* buf)
{
	while (*buf)
	{
		*buf = ascii_zx(*buf);
		++buf;
	}
  --buf;
  *buf = (*buf) + 128;
}

void terminate(unsigned char* name)
{
  name[strlen(name)-1] = name[strlen(name)-1] + 128;
}

unsigned char* zstrend(unsigned char* p)
{
  while(*p < 128) {
    ++p;
  }
  return p;
}

unsigned char* zstrcpy(unsigned char* dest, char* str)
{
  int n = strlen(str);
  strcpy(dest, str);
  cvtcmd(dest);
}


int __FASTCALL__ zxpandCommand(unsigned char* cmdbuf)
{
  #asm
  push  hl
  #endasm

  #asm
  pop   de
  call  $1ff2
  ld    a,(16445)
  ld    h,0
  ld    l,a
  #endasm
}


void initMidi(void)
{
  strcpy(0x8000, "ope mid");
  cvtcmd(0x8000);
  zxpandCommand(0x8000);
  #asm
  ld    bc,$0007
  ld    a,1
  out   (c),a ; prep write
  #endasm
}

void midiOut(uint8_t x)  __z88dk_fastcall __naked
{
  #asm
  ld    bc,$4007
  out   (c),l
  ret
  #endasm
}

void flushMidi() __naked
{
  #asm
  ; send data buffer to serial
  ld    bc,$e007
  ld    a,$c0
  out   (c),a

  ; wait until done
  call  $1ff6

  ; prep write / reset buffer
  ld    bc,$0007
  ld    a,1
  out   (c),a

  ret
  #endasm
}


  union {
    uint8_t parts[4];
    uint32_t whole32;
    uint16_t whole16;
  } longval;


// Read a 16 bits integer
uint16_t read16(void) __z88dk_fastcall
{
  longval.parts[1] = SDgetc();
  longval.parts[0] = SDgetc();
  return longval.whole16;
}


// Read a 32 bits integer
uint32_t read32(void) __z88dk_fastcall
{
  longval.parts[3] = SDgetc();
  longval.parts[2] = SDgetc();
  longval.parts[1] = SDgetc();
  longval.parts[0] = SDgetc();
  return longval.whole32;
}

/*
// Read a byte but stops if size of the track is excessed
uint8_t readTrackByte(void) __z88dk_fastcall
{
  uint8_t c = 0;
  if( tpos < miditrack.length )
  {
    c = SDgetc();
    tpos++;
  }
  return c;
}
*/

// Read a MIDI "variable length" integer
uint32_t readVariableLength() __z88dk_fastcall
{
  uint32_t v = 0;
  uint8_t c;
  //c = readTrackByte();
  c = SDgetc(); ++tpos;
  v = c & 0x7F;
  while( c & 0x80 )
  {
    //c = readTrackByte();
    c = SDgetc(); ++tpos;
    v = ( v << 7 ) | ( c & 0x7F );
  }
  return v;
}


// Read "midievent.nbdata" bytes in "midievent.data[]" starting at "midievent.data[start]"
// midievent.nbdata is not limited but we will store only "maxdata" and discard extra data
uint8_t readNdata( uint8_t start )
{
  uint32_t i;
  uint8_t c;
  for( i=start; i<midievent.nbdata; i++ )
  {
    //c = readTrackByte();
    c = SDgetc(); ++tpos;
    if( i < maxdata ) midievent.data[i] = c;
  }
  return 0;
}


// Send "All Sound Off" message to MIDI out
void allSoundOff(void)
{
  for( uint8_t i=0x00; i<=0x0F; i++ )
  {
    midiOut( 0xB0 | i );  // command: Channel Mode Message
    midiOut( 0x78 );      // data1:   All sounds Off : 0x78=120
    midiOut( 0x00 );      // data2:   "0"
    midiOut( 0xB0 | i );  // command: Channel Mode Message
    midiOut( 0x7B );      // data1:   All Notes  Off : 0x7B=123
    midiOut( 0x00 );      // data2:   "0"
  }
}


// Read MIDI file header Chunk
uint8_t readHeaderChunk(void)
{
  midiheader.chk[0] = SDgetc();
  midiheader.chk[1] = SDgetc();
  midiheader.chk[2] = SDgetc();
  midiheader.chk[3] = SDgetc();
  midiheader.length   = read32();
  midiheader.format   = read16();
  midiheader.ntracks  = read16();
  midiheader.division = read16();

  tempo = 500000; // Default tempo : 500000 microsec / beat

  if (midiheader.chk[0]=='M' && midiheader.chk[1]=='T' && midiheader.chk[2]=='h' && midiheader.chk[3]=='d' && midiheader.length == 6) {
    return 0;
  }

  return badFileheader;
}


// Read MIDI file track Chunk
uint8_t readTrackChunk(void)
{
  miditrack.chk[0] = SDgetc();
  miditrack.chk[1] = SDgetc();
  miditrack.chk[2] = SDgetc();
  miditrack.chk[3] = SDgetc();
  miditrack.length  = read32();

  if (miditrack.chk[0]=='M' && miditrack.chk[1]=='T' && miditrack.chk[2]=='r' && miditrack.chk[3]=='k' ) {
    return 0;
  }

  return badTrackheader;
}


// Read MIDI file track event
uint8_t readTrackEvent(void)
{
  uint8_t c;
  uint32_t ms;
  uint32_t time, buttonDelay=0;
  // Read time
  midievent.wait = readVariableLength();
  // Read track event
  //midievent.event = readTrackByte();
  midievent.event = SDgetc(); ++tpos;

  if( midievent.event == 0xFF )
  {
    // Meta event
    // read Meta event type
    //midievent.mtype = readTrackByte();
    midievent.mtype = SDgetc(); ++tpos;
    // read data length
    midievent.nbdata = readVariableLength();
    // read data
    readNdata(0);
    if( midievent.mtype == MF_Meta_Tempo ) // tempo
    {
      tempo = midievent.data[0] * 65536 + midievent.data[1] * 256 + midievent.data[2];
   }
  }
  else if( midievent.event == 0XF0 || midievent.event == 0xF7 )
  {
    // SysEx event
    midievent.nbdata = 0;
    do
    {
      // read one byte
      //c = readTrackByte();
      c = SDgetc(); ++tpos;
      if( midievent.nbdata < maxdata ) midievent.data[midievent.nbdata++] = c;
    } while( c != 0xF7 && tpos < miditrack.length );
  }
  else if( midievent.event & 0x80 )
  {
    // Midi event
    runningEvent = midievent.event;
    // calculate the number of data bytes
    midievent.nbdata = ( (midievent.event & 0xE0) == 0xC0 ? 1 : 2 );
    // Read data bytes
    readNdata(0);
  }
  else
  {
    // Running event
    // transfer first byte from event to data
    midievent.data[0] = midievent.event;
    // recall last event value
    midievent.event = runningEvent;
    // calculate the number of data bytes
    midievent.nbdata = ( (runningEvent & 0xE0) == 0xC0 ? 1 : 2 );
    // Read data bytes (starting from the second one since the first byte is alread in data)
    readNdata(1);
  }

  // Calculate next time on which data shall be played
  ms = ( midievent.wait * tempo ) / midiheader.division / 1000;
  nextTime += ms;

  // Output to MIDI device
  if(  midievent.event != 0xFF )
  {
    if (millis != nextTime) {
      // send queued midi & reset midi buffer
      flushMidi();
      //usleep(ms * 1000);
      millis = nextTime;
    }
    midiOut( midievent.event );
    for( uint32_t i=0; i<midievent.nbdata && i<maxdata; i++ ) {
      midiOut( midievent.data[i] );
    }
  }
  return NoError;
}


// Read MIDI file (main part)
void readMidi(void)
{
  uint16_t i;
  uint8_t err;

  // Setup MIDI device
  initMidi();

  // Read File header Chunk
  err = readHeaderChunk();
  if (err) printf("err reading header chunk");

  // Read succesive Tracks
  for( i=1; i <= midiheader.ntracks && !err; i++ )
  {
    // Read track header Chunk
    err = readTrackChunk();
   if (err) printf("err reading track chunk");

    // Read succesive Events
    for( tpos=0; tpos < miditrack.length && !err; ) 
		{
			err = readTrackEvent();
      if (err) printf("err reading track event");
		}
  }
}


int errorr(const char* errMsg, int code)
{
    puts(errMsg);
    puts("example usage: load \"mp:file\"");
    return code;
}

int main( int argc, char** argv )
{
  int nchars;
  int retCode;
  unsigned char* meta = 16444; // pr_buff

  memset(0x8000,0,0x80);

  const char* fname = 0x8008;
  strcpy(0x8000, "ope fil   ");
  cvtcmd(0x8000);

  strcpy(0x8080, "get par *32776");
  cvtcmd(0x8080);

  retCode = zxpandCommand(0x8080);
  if (retCode != 0x40) {
    return errorr("failed to retrieve parameter.", retCode & 0x3f);
  }

  terminate(fname);

  retCode = zxpandCommand(0x8000);
  if (retCode != 0x40) {
    unsigned char* p = zstrend(0x8000);
    *p = *p ^ 128;
    p = zstrcpy(p+1, ".mid");
    *p = *p ^ 128;
    retCode = zxpandCommand(0x8000);
  }

  if (retCode != 0x40) {
    return errorr("failed to open file", retCode & 0x3f);
  }

  readMidi();
  allSoundOff();

  return 0;
}
