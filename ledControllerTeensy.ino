#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

#define PIN 6

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

// Display blue and white or red and green?
bool blueWhiteMode = true;

// Automatically cycle between light patterns
bool automaticMode = true;

#define AUTOMATIC_CYCLE_INTERVAL_SEC 30
#define BRIGHTENING 0

typedef enum cmd_t
{
    FADE = 0,
    BLINK,
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
    MAX_CMDS,
} cmd_t;


// Current pixel status, used for twinkle
typedef struct current_status_t
{
    uint8_t pixelId;
    uint8_t offCount;
    uint8_t currentOffCount;
    uint8_t brightness;
    uint8_t color;
    uint8_t currDirection;
} current_status_t;


uint8_t currentPattern = FADE;

// Has the pattern JUST changed?
bool patternChanged = false;

void setup()
{
    // This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
//  #if defined (__AVR_ATtiny85__)
//    if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
//  #endif
    // End of trinket special code

    Serial.begin( 9600 ); // USB is always 12 Mbit/sec
    Serial2.begin( 9600 );
    randomSeed( analogRead( 0 ) );
    strip.begin();
    strip.setBrightness( 128 );
    strip.show(); // Initialize all pixels to 'off'
}

void processSerial()
{
    while( Serial2.available() > 0 )
    {
        char c = Serial2.read();
        Serial.print( "Got Command: " );
        Serial.println( c, DEC );

        switch( c )
        {
            case FADE:
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

            case TOGGLE_BLUE:
                blueWhiteMode = !blueWhiteMode;
                break;
            
            case BLUE_MODE:
                blueWhiteMode = true;
                break;
            
            case RED_MODE:
                blueWhiteMode = false;
                break;
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

                if( currentPattern == 0 )
                {
                  blueWhiteMode = !blueWhiteMode;
                }
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
        case FADE:
            christmasFade( 1 );
            break;

        case BLINK:
            christmasBlink( 500 );
            break;

        case THEATER:
            christmasTheaterChase( 50 );
            break;

        case SOLID:
            christmasSolid( 10 );
            break;

        case TWINKLE:
            christmasTwinkle( 1, 60, 1000 );
            break;

        default:
            christmasFade( 1 );
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

void christmasTwinkle( uint8_t wait, uint8_t nPixels, uint8_t maxOffTime )
{
    bool randomPixels = true;

    // Turn off all the pixels
    for( int i = 0; i < strip.numPixels(); i++ )
    {
        strip.setPixelColor( i, 0, 0, 0 );
    }

    strip.show();

    if( nPixels == strip.numPixels() )
    {
        randomPixels = true;
    }

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
                    status[j].pixelId = getRandomPixelId( status, nPixels );
                    status[j].brightness++;
                    status[j].currDirection = BRIGHTENING;
                    status[j].currentOffCount = 0;
                }
            }
            else
            {
                // Brighten light
                if( status[j].currDirection == BRIGHTENING )
                {
                    if( status[j].brightness >= 255 )
                    {
                        status[j].currDirection = !BRIGHTENING;
                    }
                    else
                    {
                        status[j].brightness++;
                    }
                }
                // Dim light
                else
                {
                    status[j].brightness--;
                }
            }

            if( blueWhiteMode )
            {
                strip.setPixelColor( status[j].pixelId, ( status[j].color == 0 ) ? status[j].brightness : 0, ( status[j].color == 0 ) ? status[j].brightness : 0, status[j].brightness );
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
void christmasSolid( uint8_t wait )
{
    uint16_t i, j;

    while( !stopPattern() )
    {
        for( i = 0; i < strip.numPixels(); i++ )
        {

            if( blueWhiteMode )
            {
                strip.setPixelColor( i, ( i % 2 == 0 ) ? 255 : 0, ( i % 2 == 0 ) ? 255 : 0, 255 );
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
void christmasTheaterChase( uint8_t wait )
{
    while( !stopPattern() )
    {
        for ( int q = 0; q < 3; q++ )
        {
            bool color1 = false;

            for ( uint16_t i = 0; i < strip.numPixels(); i = i + 3 )
            {
                if( blueWhiteMode )
                {
                    strip.setPixelColor( i + q, color1 ? 255 : 0, color1 ? 255 : 0, 255 ); //turn every third pixel on
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

// Lights fade, alternating between two colors
// For example, lights will start alternating red and green (pixel 0 == red, pixel 1 == gren)
// Slowly, pixel 0 will fade to green and pixel 2 will fade to red
// And on and on...
void christmasFade( uint8_t wait )
{
    uint16_t i, j;
    bool direction = false;
    int level1 = 255;
    int level2 = 0;

    j = 0;

    while( !stopPattern() )
    {

        for( i = 0; i < strip.numPixels(); i++ )
        {
            if( blueWhiteMode )
            {
                strip.setPixelColor( i, ( i % 2 == 0 ) ? level1 : level2, ( i % 2 == 0 ) ? level1 : level2, 255 );
            }
            else
            {
                strip.setPixelColor( i, ( i % 2 == 0 ) ? level1 : level2, ( i % 2 == 0 ) ? level2 : level1, 0 );
            }

        }

        strip.show();

        if( direction )
        {
            level1++;
        }
        else
        {
            level1--;
        }

        level2 = 255 - level1;

        //Serial.print(level1);
        //Serial.print(" ");
        //Serial.println(level2);

        if( level2 == 255 || level1 == 255 )
        {
            for( int i = 0; ( i < 5000 ) && ( !stopPattern ); i++ )
            {
                processSerial();
                delay( 1 );
            }
        }

        delay( wait );
        processSerial();

        if( j == 254 )
        {
            direction = !direction;
            j = 0;
        }

        j++;
    }
}

// Flash wildly
// Might be cool to randomly choose the pixel color
void christmasBlink( uint8_t wait )
{
    uint16_t i, j;

    while( !stopPattern() )
    {
        for( i = 0; i < strip.numPixels(); i++ )
        {
            if( blueWhiteMode )
            {
                strip.setPixelColor( i, ( i % 2 == 0 ) ? 255 : 0, ( i % 2 == 0 ) ? 255 : 0, 255 );
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


