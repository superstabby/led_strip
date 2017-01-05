#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

#define PIN 17

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel( 150, PIN, NEO_GRB + NEO_KHZ800 );

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.
typedef enum color_mode_t
{
    MODE_CHRISTMAS,
    MODE_WHITE,
    MODE_BLUE,
} color_mode_t;

uint8_t colorMode = MODE_WHITE;
bool automaticMode = false;

#define AUTOMATIC_CYCLE_INTERVAL_SEC 30
#define BRIGHTENING 0

typedef enum cmd_t
{
    BLINK = 0,
    THEATER,
    SOLID,
    TWINKLE,
    NUM_PATTERNS,
    AUTOMATIC = 100,
    BRIGHTNESS_LOW = 101,
    BRIGHTNESS_MED = 102,
    BRIGHTNESS_HIGH = 103,
    TOGGLE_BLUE = 104,
    BLUE_MODE = 105,
    RED_MODE = 106,
    WHITE_MODE = 107,
    MAX_CMDS,
} cmd_t;


// Current pixel status, used for twinkle
typedef struct current_status_t
{
    uint8_t pixelId;
    uint8_t offCount;
    uint8_t currentOffCount;
    int brightness;
    uint8_t color;
    uint8_t currDirection;
} current_status_t;


uint8_t currentPattern = TWINKLE;

// Has the pattern JUST changed?
bool patternChanged = false;

void setup()
{
    Serial.begin( 9600 ); // USB is always 12 Mbit/sec
    Serial2.begin( 9600 );
    randomSeed( analogRead( 0 ) );
    strip.begin();
    strip.setBrightness( 128 );
    strip.show(); // Initialize all pixels to 'off'
}

void processSerial()
{
  static int callCount = 0;
  callCount++;

  if( callCount % 10 == 0)
  {
    while( Serial2.available() > 0 )
    {
        char c = Serial2.read();
        Serial.print( "Got Command: " );
        Serial.println( c, DEC );

        switch( c )
        {
            case BLINK:
            case THEATER:
            case SOLID:
            case TWINKLE:
                currentPattern = c;
                automaticMode = false;
                patternChanged = true;
                break;

            case AUTOMATIC:
                automaticMode = true;
                currentPattern = 0;
                patternChanged = true;
                break;

            case BRIGHTNESS_LOW:
                strip.setBrightness( 50 );
                break;

            case BRIGHTNESS_MED:
                strip.setBrightness( 128 );
                break;

            case BRIGHTNESS_HIGH:
                strip.setBrightness( 255 );
                break;
            
            case BLUE_MODE:
                colorMode = MODE_BLUE;
                break;
            
            case RED_MODE:
                colorMode = MODE_CHRISTMAS;
                break;
                
            case WHITE_MODE:
                colorMode = MODE_WHITE;
                break;
        }
    }
  }
}


// This is an attempt to counter for the simplicity of the arduino language
// No threads or signals or anything
// So each pattern will check this status as often as it can
// Then it will know when to duck out so the next pattern can be run
bool stopPattern()
{
    // If the pattern has been manually changed by user
    bool retVal = patternChanged;

    // Last time the pattern changed (for automatic mode)
    static int lastPatternTime = millis();

    // If the pattern was changed by a user, reset the flag
    if( patternChanged )
    {
        patternChanged = false;

        if( automaticMode )
        {
            lastPatternTime = millis();
        }
    }
    else
    {
        // If automatic mode, change the pattern if enough time has elapsed
        if( automaticMode )
        {
            //Serial.print("Time: ");
            //Serial.println( (millis() - lastPatternTime)/1000 );
            if( ( ( millis() - lastPatternTime ) / 1000 ) >= AUTOMATIC_CYCLE_INTERVAL_SEC )
            {
                Serial.print( "Switching pattern: " );
                currentPattern = ( currentPattern + 1 ) % NUM_PATTERNS;
                Serial.println( currentPattern );
                lastPatternTime = millis();
                retVal = true;
            }
        }
    }

    return retVal;
}

// Not much of a loop
// Each pattern function will block until it's time expires (in auto mode) or a user manually changes it
void loop()
{
    // Check for new serial data from beaglebone
    processSerial();

    switch( currentPattern )
    {
        case BLINK:
            blink( 500 );
            break;

        case THEATER:
            theaterChase( 50 );
            break;

        case SOLID:
            solid( 10 );
            break;

        case TWINKLE:
            twinkle( 10, 60, 1000 );
            break;

        default:
            twinkle( 10,60,1000 );
            break;
    }
}

//-----------------------------
// TWINKLE FUNCTIONS AND HELPERS
//-----------------------------
// Twinkle mode will fade random pixels in and out
// When the function is called, it chooses n unique random pixels to fade
// When a pixel raches 0 brightness, a new unused pixel is chosen

// Has this pixel already been used?
bool idInPixelArr( current_status_t *statusArr, uint8_t nElements, uint8_t id )
{
    for( int i = 0; i < nElements; i++ )
    {
        if( statusArr[i].pixelId == id )
        {
            return true;
        }
    }

    return false;
}

// Get a new unique random pixel
uint8_t getRandomPixelId( current_status_t *statusArr, uint8_t nElements )
{
    uint8_t id = random( 0, strip.numPixels() + 1 );

    while( idInPixelArr( statusArr, nElements, id ) )
    {
        id = random( 0, strip.numPixels() + 1 );
    }

    return id;
}

void twinkle( uint8_t wait, uint8_t nPixels, uint8_t maxOffTime )
{
    bool randomPixels = false;
    nPixels = strip.numPixels();

    // Turn off all the pixels
    for( int i = 0; i < strip.numPixels(); i++ )
    {
        strip.setPixelColor( i, 0, 0, 0 );
    }

    strip.show();

//    if( nPixels != strip.numPixels() )
//    {
//        randomPixels = true;
//    }

    current_status_t status[nPixels];


    // Initialize
    for( int i = 0; i < nPixels; i++ )
    {
        if( !randomPixels )
        {
            status[i].pixelId = i;
        }
        else
        {
            // Find a random pixel ID
            status[i].pixelId = getRandomPixelId( status, i );
        }

        status[i].offCount = random( 100, maxOffTime + 1 );
        status[i].currentOffCount = 0;
        status[i].brightness = 0;
        status[i].color = random( 0, 2 );
        status[i].currDirection = BRIGHTENING;
    }

    //Until the user commands a pattern change
    while( !stopPattern() )
    {
        for( int j = 0; j < nPixels; j++ )
        {
            // If light is off
            // Keep light off for currentOffCount
            // Then fade it in and out
            if( status[j].brightness == 0 )
            {
                status[j].currentOffCount++;

                // If the pixel has been off for long enough
                if( status[j].currentOffCount >= status[j].offCount )
                {
                    // Start making it brighter
                    if( randomPixels)
                    {
                      status[j].pixelId = getRandomPixelId( status, nPixels );
                    }
                    status[j].brightness+=5;
                    status[j].currDirection = BRIGHTENING;
                    status[j].color = random( 0, 2 );
                    status[j].currentOffCount = 0;
                }
            }
            else
            {
                // Brighten light
                if( status[j].currDirection == BRIGHTENING )
                {
                    status[j].brightness+=5;
                    
                    if( status[j].brightness >= 255 )
                    {
                        status[j].brightness = 255;
                        status[j].currDirection = !BRIGHTENING;
                    }
                }
                // Dim light
                else
                {
                    status[j].brightness-=5;

                    if( status[j].brightness < 0 )
                    {
                      status[j].brightness = 0;
                    }
                }
            }

            if(j == 0 )
            {
              Serial.print("Brightness: ");
              Serial.println(status[j].brightness);
            }

            if( colorMode == MODE_BLUE )
            {
                strip.setPixelColor( status[j].pixelId, ( status[j].color == 0 ) ? status[j].brightness : 0, ( status[j].color == 0 ) ? status[j].brightness : 0, status[j].brightness );
            }
            else if( colorMode == MODE_WHITE )
            {
                strip.setPixelColor( status[j].pixelId, status[j].brightness, status[j].brightness, status[j].brightness );
            }
            else
            {
                strip.setPixelColor( status[j].pixelId, ( status[j].color != 0 ) ? status[j].brightness : 0, ( status[j].color == 0 ) ? status[j].brightness : 0, 0 );
            }
        }

        strip.show();
        delay( wait );
        processSerial();
    }
}

//--------------------------------------------

// Just sit there in solid mode
// TODO no reason to keep setting color
// Just need to stop sometimes to check for new serial data
void solid( uint8_t wait )
{
    uint16_t i, j;

    while( !stopPattern() )
    {
        for( i = 0; i < strip.numPixels(); i++ )
        {

            if( colorMode == MODE_BLUE )
            {
                strip.setPixelColor( i, ( i % 2 == 0 ) ? 255 : 0, ( i % 2 == 0 ) ? 255 : 0, 255 );
            }
            else if( colorMode == MODE_WHITE )
            {
              strip.setPixelColor( i, ( i % 2 == 0 ) ? 255 : 0, ( i % 2 == 0 ) ? 255 : 0, ( i % 2 == 0 ) ? 255 : 0 );
            }
            else
            {
                strip.setPixelColor( i, ( i % 2 == 0 ) ? 255 : 0, ( i % 2 == 0 ) ? 0 : 255, 0 );
            }
        }

        strip.show();
        delay( wait );
        processSerial();
    }
}

//Theatre-style crawling lights.
void theaterChase( uint8_t wait )
{
    while( !stopPattern() )
    {
        for ( int q = 0; q < 3; q++ )
        {
            bool color1 = false;

            for ( uint16_t i = 0; i < strip.numPixels(); i = i + 3 )
            {
                if( colorMode == MODE_BLUE )
                {
                    strip.setPixelColor( i + q, color1 ? 255 : 0, color1 ? 255 : 0, 255 ); //turn every third pixel on
                }
                else if( colorMode == MODE_WHITE )
                {
                    strip.setPixelColor( i + q, 255, 255, 255 ); //turn every third pixel on
                }
                else
                {
                    strip.setPixelColor( i + q, color1 ? 255 : 0, color1 ? 0 : 255, 0 ); //turn every third pixel on
                }

                color1 = !color1;
            }

            strip.show();

            delay( wait );
            processSerial();

            for ( uint16_t i = 0; i < strip.numPixels(); i = i + 3 )
            {
                strip.setPixelColor( i + q, 0 );    //turn every third pixel off
            }
        }
    }
}


// Flash wildly
// Might be cool to randomly choose the pixel color
void blink( uint8_t wait )
{
    uint16_t i, j;

    while( !stopPattern() )
    {
        for( i = 0; i < strip.numPixels(); i++ )
        {
            if( colorMode == MODE_BLUE )
            {
                strip.setPixelColor( i, ( i % 2 == 0 ) ? 255 : 0, ( i % 2 == 0 ) ? 255 : 0, 255 );
            }
            else if( colorMode == MODE_WHITE )
            {
                strip.setPixelColor( i, 255, 255, 255 );
            }
            else
            {
                strip.setPixelColor( i, ( i % 2 == 0 ) ? 255 : 0, ( i % 2 == 0 ) ? 0 : 255, 0 );
            }

        }

        strip.show();
        delay( wait );
        processSerial();

        for( i = 0; i < strip.numPixels(); i++ )
        {
            strip.setPixelColor( i, 0, 0, 0 );
        }

        strip.show();
        delay( wait );
        processSerial();
    }
}


