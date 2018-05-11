// based on https://community.atmel.com/projects/sd-card-midi-player

#include <stdio.h>
#include <unistd.h>

typedef signed char int8_t;
typedef unsigned char uint8_t;

typedef signed short int16_t;
typedef unsigned short uint16_t;

typedef signed int int32_t;
typedef unsigned int uint32_t;


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
#define MF_Meta_Track_name       0x03  // Track name
#define MF_Meta_Instrument_name  0x04  // Instrument name
#define MF_Meta_Lyric            0x05  // Lyric text
#define MF_Meta_Marker           0x06  // Marker text
#define MF_Meta_Cue_point        0x07  // Cue point
#define MF_Meta_MIDI_channel     0x20  // MIDI channel
#define MF_Meta_MIDI_Port        0x21  // MIDI Port
#define MF_Meta_Track_End        0x2F  // End of track
#define MF_Meta_Tempo            0x51  // Tempo setting
#define MF_Meta_SMPTE_offset     0x54  // SMPTE offset
#define MF_Meta_Time_signature   0x58  // Time signature
#define MF_Meta_Key_signature    0x59  // Key signature
#define MF_Meta_Special          0x7F  // Seq. special

// FILE HEADER INFORMATION
typedef struct
{
  uint8_t  chk[5];
  uint32_t length;
  uint16_t format;
  uint16_t ntracks;
  uint16_t division;
} MTHD;

// TRACK INFORMATION
typedef struct
{
  uint8_t  chk[5];
  uint32_t length;
} MTRK;

// EVENT INFORMATION
#define maxdata 127
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
  badFileHeader  = 1,
  badTrackHeader = 2,
  badEvent       = 3,
  endOfFile      = 4,
	userStop       = 5
};

// FUNCTIONS
void     clearBuffer(void);
uint8_t  readByte(void);
uint8_t  readTrackByte(void);
uint8_t  readHeaderChunk(void);
uint8_t  readTrackChunk(void);
uint16_t read16(void);
uint32_t read32(void);
uint32_t readVariableLength(void);
uint8_t  readNdata( uint8_t start );
uint8_t  readTrackEvent(void);
void     allSoundOff(void);

// Midi file, type 0
FILE* inFile;

// Position in Track
uint32_t tpos     = 0;
uint32_t prevpos  = 0;

// LAST EVENT READ
uint8_t runningEvent = 0;

// TEMPO (microsec/beat)
uint32_t Tempo = 500000;
double   TempoFactor = 1.0;

// Shared variables
MTHD Header;
MTRK Track;
MTEV Event;

uint32_t millis, nextTime = 0;


uint8_t SDgetc(void)
{
  return (uint8_t)fgetc(inFile);
}

void MIDIinit(void)
{
  // Initialise the USART for MIDI transmission
}

void MidiOut( uint8_t x )
{
  //printf("%02x", x);
  // Send a byte to the MIDI out port (Tx)
}


// Read a 16 bits integer
uint16_t read16(void)
{
  uint16_t v;
  v  = (SDgetc() << 8);
  v |= SDgetc() ;
  return v;
}


// Read a 32 bits integer
uint32_t read32(void)
{
  uint32_t v;
  v  = SDgetc();
  v = ( v << 8 ) | SDgetc();
  v = ( v << 8 ) | SDgetc();
  v = ( v << 8 ) | SDgetc();
  return v;
}


// Read a byte but stops if size of the track is excessed
uint8_t readTrackByte(void)
{
  uint8_t c = 0;
  if( tpos < Track.length )
  {
    c = SDgetc();
    tpos++;
  }
  return c;
}


// Read a MIDI "variable length" integer
uint32_t readVariableLength()
{
  uint32_t v = 0;
  uint8_t c;
  c = readTrackByte();
  v = c & 0x7F;
  while( c & 0x80 )
  {
    c = readTrackByte();
    v = ( v << 7 ) | ( c & 0x7F );
  }
  return v;
}


// Read "Event.nbdata" bytes in "Event.data[]" starting at "Event.data[start]"
// Event.nbdata is not limited but we will store only "maxdata" and discard extra data
uint8_t readNdata( uint8_t start )
{
  uint32_t i;
  uint8_t c;
  for( i=start; i<Event.nbdata; i++ )
  {
    c = readTrackByte();
    if( i < maxdata ) Event.data[i] = c;
  }
  return 0;
}


// Send "All Sound Off" message to MIDI out
void allSoundOff(void)
{
  for( uint8_t i=0x00; i<=0x0F; i++ )
  {
    MidiOut( 0xB0 | i );  // command: Channel Mode Message
    MidiOut( 0x78 );      // data1:   All sounds Off : 0x78=120
    MidiOut( 0x00 );      // data2:   "0"
    MidiOut( 0xB0 | i );  // command: Channel Mode Message
    MidiOut( 0x7B );      // data1:   All Notes  Off : 0x7B=123
    MidiOut( 0x00 );      // data2:   "0"
  }
}


// Read MIDI file Header Chunk
uint8_t readHeaderChunk(void)
{
  for( int i=0; i<4; i++ ) Header.chk[i] = SDgetc();
  Header.length   = read32();
  Header.format   = read16();
  Header.ntracks  = read16();
  Header.division = read16();
  Tempo = 500000.0; // Default tempo : 500000 microsec / beat

  printf("Type: %d\n", Header.format);
  printf("Tracks: %d\n", Header.ntracks);

  return ( Header.chk[0]=='M' && Header.chk[1]=='T' && Header.chk[2]=='h' && Header.chk[3]=='d' && Header.length==6 ? NoError : badFileHeader );
}


// Read MIDI file Track Chunk
uint8_t readTrackChunk(void)
{
  for( int i=0; i<4; i++ ) Track.chk[i] = SDgetc();
  Track.length  = read32();
  return ( Track.chk[0]=='M' && Track.chk[1]=='T' && Track.chk[2]=='r' && Track.chk[3]=='k' ? NoError : badTrackHeader );
}



// Read MIDI file Track Event
uint8_t readTrackEvent(void)
{
  uint8_t c;
  uint32_t ms;
  uint32_t IRreceived;
  uint32_t time, buttonDelay=0;
  // Read time
  Event.wait = readVariableLength();
  // Read track event
  Event.event = readTrackByte();
  if( Event.event == 0xFF )
  {
    // Meta Event
    // read Meta event type
    Event.mtype = readTrackByte();
    // read data length
    Event.nbdata = readVariableLength();
    // read data
    readNdata(0);
    if( Event.mtype == MF_Meta_Tempo ) // Tempo
    {
      Tempo = Event.data[0] * 65536 + Event.data[1] * 256 + Event.data[2];
   }
  }
  else if( Event.event == 0XF0 || Event.event == 0xF7 )
  {
    // SysEx Event
    Event.nbdata = 0;
    do
    {
      // read one byte
      c = readTrackByte();
      if( Event.nbdata < maxdata ) Event.data[Event.nbdata++] = c;
    } while( c != 0xF7 && tpos < Track.length );
  }
  else if( Event.event & 0x80 )
  {
    // Midi Event
    runningEvent = Event.event;
    // calculate the number of data bytes
    Event.nbdata = ( (Event.event & 0xE0) == 0xC0 ? 1 : 2 );
    // Read data bytes
    readNdata(0);
  }
  else
  {
    // Running Event
    // transfer first byte from event to data
    Event.data[0] = Event.event;
    // recall last event value
    Event.event = runningEvent;
    // calculate the number of data bytes
    Event.nbdata = ( (runningEvent & 0xE0) == 0xC0 ? 1 : 2 );
    // Read data bytes (starting from the second one since the first byte is alread in data)
    readNdata(1);
  }

  // Calculate next time on which data shall be played
  ms = ( Event.wait * (uint32_t)(Tempo / TempoFactor) ) / Header.division / 1000;
  nextTime += ms;

  // Output to MIDI device
  if(  Event.event != 0xFF )
  {
    if (millis != nextTime) {
      printf("\nwait till %08d\n", nextTime);
      usleep(ms * 1000);
      millis = nextTime;
    }
    MidiOut( Event.event );
    for( uint32_t i=0; i<Event.nbdata && i<maxdata; i++ ) MidiOut( Event.data[i] );
  }
  return NoError;
}


// Read MIDI file (main part)
void readMidi(void)
{
  uint16_t i;
  uint8_t err;

  // Setup MIDI device
  MIDIinit();

  // Read File Header Chunk
  err = readHeaderChunk();

  // Read succesive Tracks
  for( i=1; i<=Header.ntracks && !err; i++ )
  {
    // Read Track Header Chunk
    err = readTrackChunk();

    // Read succesive Events
    for( tpos=0; tpos < Track.length && !err; ) 
		{
			err = readTrackEvent();
		}
  }
}


int main( int argc, char** argv )
{
  inFile = fopen(argv[1], "rb");
  if (inFile != NULL) {
    readMidi();
    allSoundOff();
  }
  return 0;
}
