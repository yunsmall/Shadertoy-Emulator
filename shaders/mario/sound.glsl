// Overworld theme based on notes from geoffrey1218's famitracker cover
// Level clear theme based on notes from WheeljackDude's famitracker cover

float SquareWave25( float f, float x )
{
    return floor( 4.0 * floor( f * x ) - floor( 4.0 * f * x ) + 1.0 );
}

float SquareWave50( float f, float x )
{
    return floor( 2.0 * floor( f * x ) - floor( 2.0 * f * x ) + 1.0 );
}

float SinWave( float f, float x )
{
    return sin( f * x * 2.0 * 3.14 );
}

float SawtoothWave( float q, float x )
{
    float f = fract( x ) - q;
    f /= (f >= 0.0 ? 1.0 : 0.0) - q;
    return f * 2.0 - 1.0;
}

float Noise( float x )
{
    return fract( sin( 123523.9898 * x ) * 43758.5453 );
}

float Jump( float t )
{
    if ( t < 0.0 || t > 0.5 )
    {
        return 0.0;
    }
    

	float freq = mix( 250.0, 330.0, t / 0.5 );
    
    float waveSwitchT = 0.068;
    if ( t < waveSwitchT ) 
    {
        return SquareWave50( freq, t ) * 0.5;
    }
    else
    {
        return SquareWave25( freq, t ) * clamp( 1.0 - ( t - 0.1 ) / ( 0.5 - 0.1 ), 0.0, 1.0 ) * 0.5;
    }
}

float Stomp( float t )
{
    if ( t < 0.0 || t > 0.25 )
    {
        return 0.0;
    }
 
    float freq = mix( 200.0, 300.0, t / 0.2 );

    if ( t <= 0.1 )
    {
    	return SquareWave50( freq, t ) * clamp( t / 0.1, 0.0, 1.0 );
    }
    else
    {
        return SquareWave50( freq, t ) * clamp( ( t - 0.1 ) / 0.15, 0.0, 1.0 );
    }
}

float PowerUp( float t )
{
    if ( t < 0.0 || t > 0.9 )
    {
        return 0.0;
    }
    
    float freq = 250.0 + mod( t, 0.3 ) * 50.0;
    return mix( SquareWave50( freq, t ), SquareWave25( freq, t ), mod( t, 0.3 ) > 0.2 ? 0.5 : 0.0 ) * 0.5;
}

float Bump( float t )
{
    if ( t < 0.0 || t > 0.15 )
    {
        return 0.0;
    }
    
    float freq = mix( 150.0, 130.0, abs( t - 0.075 ) / 0.075 );
    return SquareWave50( freq, t ) * 0.5 + SinWave( freq, t ) * 0.5;
}

float Coin( float t )
{
    if ( t < 0.0 || t > 0.75 )
    {
        return 0.0;
    }    
    
    float freq = t > 0.1 ? 600.0 : 480.0;
    return SquareWave50( freq, t ) * clamp( 1.0 - ( t - 0.1 ) / 0.65, 0.0, 1.0 );
}

float DownTheFlagpole( float t )
{
    if ( t < 0.0 || t > 1.0 )
    {
        return 0.0;
    } 
    
    float freq = floor( mix( 200.0, 600.0, t ) * 0.1 ) * 10.0;
	return SquareWave50( freq, t ) * clamp( 1.0 - ( t - 0.95 ) / 0.05, 0.0, 1.0 );
}

float InstrumentMain( float f, float t )
{
    float ret = 0.0;
    ret += SquareWave50( f, t ) * clamp( 1.0 - t / 0.250, 0.0, 1.0 ) * 0.5;
    return ret;
}

float InstrumentBass( float f, float t )
{
	return SinWave( f, t ) * clamp( 1.0 - ( t - 0.1 ) / 0.01, 0.0, 1.0 );
}

float InstrumentDrums( float f, float t )
{
    if ( f == 1.0 )
    {
        // open
    	return Noise( t ) * clamp( 1.0 - ( t - 0.08 ) / ( 0.001 ), 0.0, 1.0 ) * 0.66;  
    }
    else if ( f == 2.0 )
    {
        // close
        return Noise( t ) * clamp( 1.0 - ( t - 0.0165 ) / ( 0.001 ), 0.0, 1.0 ) * 0.66;
    }
    else
    {
    	// kick
        return SquareWave50( 100.0, t ) * clamp( 1.0 - ( t - 0.0165 ) / ( 0.001 ), 0.0, 1.0 );
    }
}

// note frequencies
const float E2	=  82.41;
const float F2	=  87.31;
const float G2 	=  98.00;
const float GH2 = 103.83;
const float AH2 = 116.54;
const float C3 	= 130.81;
const float CH3	= 138.59;
const float D3 	= 146.83;
const float DH3 = 155.56;
const float E3 	= 164.81;
const float F3	= 174.61;
const float FH3 = 185.00;
const float G3 	= 196.00;
const float GH3 = 207.65;
const float A3 	= 220.00;
const float AH3	= 233.08;
const float B3 	= 246.94;
const float C4 	= 261.63;
const float D4  = 293.66;
const float DH4 = 311.13;
const float E4 	= 329.63;
const float F4 	= 349.23;
const float FH4 = 369.99;
const float G4 	= 392.00;
const float GH4 = 415.30;
const float A4	= 440.00;
const float AH4 = 466.16;
const float C5  = 523.25;
const float G5  = 783.99;

#define N( off, freq ) 	if( pos > patternPos + float( off ) ) { notePos = patternPos + float( off ); noteFreq = float( freq ); }
#define PEND			patternPos += 32.0;

const float TimeToPos = 1000.0 / 75.0;
const float PosToTime = 1.0 / TimeToPos;

float OverworldMelody( float time )
{
    float pos			= time * TimeToPos;
    float noteFreq 		= 0.0;
    float notePos		= 0.0;
    float patternPos	= 0.0;
    
#define P0  N(0,E4)N(2,E4)N(6,E4)N(10,C4)N(12,E4)N(16,G4)PEND
#define P1  N(0,C4)N(6,G3)N(12,E3)N(18,A3)N(22,B3)N(26,AH3)N(28,A3)PEND
#define P2  N(0,G3)N(3,E4)N(6,G4)N(8,A4)N(12,F4)N(14,G4)N(18,E4)N(22,C4)N(24,D4)N(26,B3)PEND
#define P3  N(4,G4)N(6,FH4)N(8,F4)N(10,DH4)N(14,E4)N(18,GH3)N(20,A3)N(22,C4)N(26,A3)N(28,C4)N(30,D4)PEND
#define P4  N(4,G4)N(6,FH4)N(8,F4)N(10,DH4)N(14,E4)N(18,C5)N(22,C5)N(24,C5)PEND
#define P5  N(4,DH4)N(10,D4)N(16,C4)PEND
#define P6  N(0,C4)N(2,C4)N(6,C4)N(10,C4)N(12,D4)N(16,E4)N(18,C4)N(22,A3)N(24,G3)PEND
#define P7  N(0,C4)N(2,C4)N(6,C4)N(10,C4)N(12,D4)N(14,E4)PEND
    
    P0 P1 P2 P1 P2 P3 P4 P3 P5 P3 
    P4 P3 P5 P6 P7 P6 P0 P1 P2 P1
        
#undef P0
#undef P1
#undef P2
#undef P3        
#undef P4        
#undef P5
#undef P6
#undef P7

	return InstrumentMain( noteFreq, ( pos - notePos ) * PosToTime );
}

float OverworldHarmony( float time )
{
    float pos			= time * TimeToPos;
    float noteFreq 		= 0.0;
    float notePos		= 0.0;
    float patternPos	= 0.0;
    
#define P0  N(0,FH3)N(2,FH3)N(6,FH3)N(10,FH3)N(12,FH3)N(16,B3)N(24,G3)PEND
#define P1  N(0,E3)N(6,C3)N(12,G2)N(18,C3)N(22,D3)N(26,CH3)N(28,C3)PEND
#define P2  N(0,C3)N(3,G3)N(6,B3)N(8,C4)N(12,A3)N(14,B3)N(18,A3)N(22,E3)N(24,F3)N(26,D3)PEND    
#define P3  N(4,E4)N(6,DH4)N(8,D4)N(10,B3)N(14,C4)N(18,E3)N(20,F3)N(22,G3)N(26,C3)N(28,E3)N(30,F3)PEND    
#define P4  N(4,E4)N(6,DH4)N(8,D4)N(10,B3)N(14,C4)N(18,F4)N(22,F4)N(24,F4)PEND
#define P5  N(4,GH3)N(10,F3)N(16,E3)PEND
#define P6  N(0,GH3)N(2,GH3)N(6,GH3)N(10,GH3)N(12,AH3)N(16,G3)N(18,E3)N(22,E3)N(24,C3)PEND
#define P7  N(0,GH3)N(2,GH3)N(6,GH3)N(10,GH3)N(12,AH3)N(14,G3)PEND
    
    P0 P1 P2 P1 P2 P3 P4 P3 P5 P3 
    P4 P3 P5 P6 P7 P6 P0 P1 P2 P1
        
#undef P0
#undef P1
#undef P2
#undef P3        
#undef P4        
#undef P5
#undef P6
#undef P7

    return InstrumentMain( noteFreq, ( pos - notePos ) * PosToTime );
}


float OverworldBass( float time )
{
    float pos			= time * TimeToPos;
    float noteFreq 		= 0.0;
    float notePos		= 0.0;
    float patternPos	= 0.0;
    

#define P0 N(0,D3)N(2,D3)N(6,D3)N(10,D3)N(12,D3)N(16,G4)N(24,G3)PEND
#define P1 N(0,G3)N(6,E3)N(12,C3)N(18,F3)N(22,G3)N(26,FH3)N(28,F3)PEND
#define P2 N(0,E3)N(3,C4)N(6,E4)N(8,F4)N(12,D4)N(14,E4)N(18,C4)N(22,A3)N(24,B3)N(26,G3)PEND    
#define P3 N(0,C3)N(6,G3)N(12,C4)N(16,F3)N(22,C4)N(24,C4)N(28,F3)PEND
#define P4 N(0,C3)N(6,E3)N(12,G3)N(14,C4)N(18,G5)N(22,G5)N(24,G5)N(28,G3)PEND
#define P5 N(0,C3)N(4,GH3)N(10,B3)N(16,C4)N(16,C4)N(22,G3)N(24,G3)N(28,C3)PEND
#define P6 N(0,GH2)N(6,DH3)N(12,GH3)N(16,G3)N(22,C3)N(28,G2)PEND
    
    P0 P1 P2 P1 P2 P3 P4 P3 P5 P3 
    P4 P3 P5 P6 P6 P6 P0 P1 P2 P1
        
#undef P0
#undef P1
#undef P2
#undef P3        
#undef P4        
#undef P5
#undef P6        

    return InstrumentBass( noteFreq, ( pos - notePos ) * PosToTime );
}

float OverworldDrums( float time )
{
    float pos			= time * TimeToPos;
    float noteFreq 		= 0.0;
    float notePos		= 0.0;
    float patternPos	= 0.0;
    
    // 1 - open
    // 2 - close
    // 3 - kick
#define P0 N(0,1)N(4,2)N(6,1)N(10,2)N(12,1)N(16,1)N(24,1)N(26,2)N(28,2)N(30,2)PEND
#define P1 N(0,3)N(4,2)N(7,2)N(8,1)N(12,2)N(15,2)N(16,3)N(20,2)N(23,2)N(24,1)N(28,2)N(31,2)PEND
#define P2 N(0,1)N(4,2)N(6,1)N(10,2)N(12,1)N(15,1)N(22,1)N(26,2)N(28,2)N(30,2)PEND    
    
    P0 P1 P1 P1 P1 P1 P1 P1 P1 P1
    P1 P1 P1 P2 P2 P2 P0 P1 P1 P1
        
#undef P0
#undef P1        
#undef P2
    
    return InstrumentDrums( noteFreq, ( pos - notePos ) * PosToTime );
}

const float TimeToPos2 = 0.5 * TimeToPos;
const float PosToTime2 = 1.0 / TimeToPos2;

float LevelClearMelody( float time )
{
    float pos			= time * TimeToPos2;
    float noteFreq 		= 0.0;
    float notePos		= 0.0;
    float patternPos	= 0.0;
    
	N(0,G2)N(1,C3)N(2,E3)N(3,G3)N(4,C4)N(5,E4)N(6,G4)N(9,E4)N(12,GH2)N(13,C3)N(14,DH3)N(15,GH3)N(16,C4)N(17,DH4)N(18,GH4)N(21,DH4)N(24,AH2)N(25,DH3)N(26,F3)N(27,AH3)N(28,D4)N(29,F4)N(30,AH4)N(33,AH4)N(34,AH4)N(35,AH4)N(36,E4)

    return InstrumentMain( noteFreq, ( pos - notePos ) * PosToTime2 );
}

float LevelClearHarmony( float time )
{
    float pos			= time * TimeToPos2;
    float noteFreq 		= 0.0;
    float notePos		= 0.0;
    float patternPos	= 0.0;
    
	N(1,E2)N(2,G2)N(3,C3)N(4,E3)N(5,G3)N(6,C4)N(9,G3)N(13,E2)N(14,GH2)N(15,C3)N(16,DH3)N(17,GH3)N(18,C4)N(21,GH3)N(25,F2)N(26,AH2)N(27,D3)N(28,F3)N(29,AH3)N(30,D4)N(33,D4)N(36,C5)

    return InstrumentMain( noteFreq, ( pos - notePos ) * PosToTime2 );
}

float LevelClearBass( float time )
{
    float pos			= time * TimeToPos2;
    float noteFreq 		= 0.0;
    float notePos		= 0.0;
    float patternPos	= 0.0;
    
	N(3,C3)N(4,E3)N(5,G3)N(6,E4)N(9,C4)N(15,C3)N(16,DH3)N(17,GH3)N(18,DH4)N(21,C4)N(27,D3)N(28,F3)N(29,AH3)N(30,F4)N(33,D4)N(34,D4)N(35,D4)N(36,C4)

    return InstrumentBass( noteFreq, ( pos - notePos ) * PosToTime2 );
}

float GameSounds( float time )
{
    // play sounds a bit earlier
    time += 0.1;

    float ret = 0.0;
    
    float marioBigJump1 = 27.1;
    float marioBigJump2 = 29.75;
    float marioBigJump3 = 35.05;    
    
    
    // Jump sounds
    float jumpTime = time - 38.7;
    if ( jumpTime <= 0.0 ) { jumpTime = time - marioBigJump3 - 1.2 - 0.75; }    
    if ( jumpTime <= 0.0 ) { jumpTime = time - marioBigJump3 - 1.2; }
    if ( jumpTime <= 0.0 ) { jumpTime = time - marioBigJump3; }
    if ( jumpTime <= 0.0 ) { jumpTime = time - 34.15; }    
    if ( jumpTime <= 0.0 ) { jumpTime = time - 33.7; }
    if ( jumpTime <= 0.0 ) { jumpTime = time - 32.3; }    
    if ( jumpTime <= 0.0 ) { jumpTime = time - marioBigJump2 - 1.0; }    
    if ( jumpTime <= 0.0 ) { jumpTime = time - marioBigJump2; }    
    if ( jumpTime <= 0.0 ) { jumpTime = time - marioBigJump1 - 1.0; }    
    if ( jumpTime <= 0.0 ) { jumpTime = time - marioBigJump1; }    
    if ( jumpTime <= 0.0 ) { jumpTime = time - 25.75; }    
	if ( jumpTime <= 0.0 ) { jumpTime = time - 24.7; }        
    if ( jumpTime <= 0.0 ) { jumpTime = time - 23.0; } 
    if ( jumpTime <= 0.0 ) { jumpTime = time - 21.7; }    
    if ( jumpTime <= 0.0 ) { jumpTime = time - 19.65; }   
    if ( jumpTime <= 0.0 ) { jumpTime = time - 18.7; }    
    if ( jumpTime <= 0.0 ) { jumpTime = time - 15.1; } 
    if ( jumpTime <= 0.0 ) { jumpTime = time - 13.62; }    
    if ( jumpTime <= 0.0 ) { jumpTime = time - 11.05; }
    if ( jumpTime <= 0.0 ) { jumpTime = time - 9.0; }
    if ( jumpTime <= 0.0 ) { jumpTime = time - 7.8; }
    if ( jumpTime <= 0.0 ) { jumpTime = time - 6.05; }
    if ( jumpTime <= 0.0 ) { jumpTime = time - 5.0; }
    if ( jumpTime <= 0.0 ) { jumpTime = time - 4.2; }
	ret += Jump( jumpTime );                           

    // block bump sounds
    float bumpTime = time - 33.9;
    if ( bumpTime <= 0.0 ) { bumpTime = time - 22.4; }
    if ( bumpTime <= 0.0 ) { bumpTime = time - 15.4; }
    if ( bumpTime <= 0.0 ) { bumpTime = time - 5.3; }
    ret += Bump( bumpTime );
    
    // coin sounds
    float coinTime = time - 33.9;
    if ( coinTime <= 0.0 ) { coinTime = time - 22.4; }
    if ( coinTime <= 0.0 ) { coinTime = time - 5.4; }    
    ret += Coin( coinTime );    

    float stompTime = time - 26.3;
    if ( stompTime <= 0.0 ) { stompTime = time - 25.3; }
    if ( stompTime <= 0.0 ) { stompTime = time - 23.5; }    
    if ( stompTime <= 0.0 ) { stompTime = time - 20.29; }    
    if ( stompTime <= 0.0 ) { stompTime = time - 10.3; }    
    ret += Stomp( stompTime );
    
	ret += PowerUp( time - 17.0 );    

    ret += DownTheFlagpole( time - 38.95 );    
    
    return ret;
}

vec2 mainSound( in int samp, float time )
{    
    float ret = 0.0;
    
    float overworldTime  = max( time -  1.0, 0.0 );
    float levelClearTime = max( time - 40.2, 0.0 );

    ret += OverworldMelody( overworldTime ) 	* 0.3;
    ret += OverworldHarmony( overworldTime ) 	* 0.2;
    ret += OverworldBass( overworldTime )   	* 0.2;
    ret += OverworldDrums( overworldTime )		* 0.15;  
    
    // overworld theme fadout before the castle
    ret *= 1.0 - smoothstep( 38.0, 40.0, time );
    
    ret += LevelClearMelody( levelClearTime ) 	* 0.3;
    ret += LevelClearHarmony( levelClearTime ) 	* 0.3;
    ret += LevelClearBass( levelClearTime ) 	* 0.3;
    
    ret += GameSounds( time )					* 0.2;
    
    // disable output on first frames
    ret = time <= 1.0 ? 0.0 : ret;
    
    return vec2( ret, ret );
}