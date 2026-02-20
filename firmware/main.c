
#include "mcc_generated_files/system/system.h"
#include "mcc_generated_files/adc/adc.h"

#include <stdint.h>
#include <stdbool.h>

// INA138 parameters
#define RS_OHMS         0.5f
#define RL_OHMS        24000.0f

// Thresholds
#define LOW_CURRENT_MA      10.0f
#define RE_ENABLE_MA        20.0f
#define PERSIST_MS          3000
#define CALIBRATION_FACTOR  1.2f    // for resistor tolerance

// ADC channels
#define CH_PORT1_CURR   Port1_Curr   // ADC_CHANNEL_ANB3
#define CH_PORT2_CURR   Port2_Curr   // ADC_CHANNEL_ANB2
#define CH_PORT3_CURR   Port3_Curr   // ADC_CHANNEL_ANA3
#define CH_PORT4_CURR   Port4_Curr   // ADC_CHANNEL_ANA2


// Enable pin macros (high = ON / LED on, assuming active-high)
#define PORT1_EN_ON()   LATBbits.LATB4 = 1
#define PORT1_EN_OFF()  LATBbits.LATB4 = 0
#define PORT2_EN_ON()   LATBbits.LATB5 = 1
#define PORT2_EN_OFF()  LATBbits.LATB5 = 0
#define PORT3_EN_ON()   LATAbits.LATA0 = 1
#define PORT3_EN_OFF()  LATAbits.LATA0 = 0
#define PORT4_EN_ON()   LATAbits.LATA1 = 1
#define PORT4_EN_OFF()  LATAbits.LATA1 = 0

// CC detection (low = 0 = device attached)
#define PORT1_CC_ATTACHED()  ((PORTBbits.RB1 == 0) || (PORTCbits.RC7 == 0))   // RB1=Port1_CC2, RC7=Port1_CC1
#define PORT2_CC_ATTACHED()  ((PORTCbits.RC6 == 0) || (PORTCbits.RC4 == 0))   // RC6=Port2_CC2, RC4=Port2_CC1
#define PORT3_CC_ATTACHED()  ((PORTCbits.RC2 == 0) || (PORTCbits.RC3 == 0))   // RC2=Port3_CC1, RC3=Port3_CC2
#define PORT4_CC_ATTACHED()  ((PORTAbits.RA4 == 0) || (PORTAbits.RA5 == 0))   // RA4=Port4_CC1, RA5=Port4_CC2


// Simple delay (approximate, adjust if needed)
#define BLINK_DELAY_MS  500

//ADC defines
#define ADC_RESOLUTION 1024
#define VREF_mV 4096.0f // FVR = 4.096V

// Port state structure
typedef struct {
    bool prev_cc_attached;      // Previous CC state
    bool enabled;               // Port power enabled
    uint32_t low_timer_ms;      // Low current timer
} PortState_t;

PortState_t port_state[4] = {
    {false, false, 0},
    {false, false, 0},
    {false, false, 0},
    {false, false, 0}
};


// Read voltage (mV)
float read_vout_mV(adc_channel_t channel) {
    adc_result_t raw = ADC_ChannelSelectAndConvert(channel);
    return (raw * VREF_mV) / ADC_RESOLUTION;
}

// Calculate current (mA)
float current_from_vout(float vout_mV) {
    float raw_current = (vout_mV * 1000.0f) / (RS_OHMS * RL_OHMS);
    return raw_current * CALIBRATION_FACTOR;
}

float getCurrent_mA(adc_channel_t channel) {
    float vout = read_vout_mV(channel);
    return current_from_vout(vout);
}


void blink_all_three_times(void) {
    for (int i = 0; i < 2; i++) {
        // All ON
        PORT1_EN_ON();
        PORT2_EN_ON();
        PORT3_EN_ON();
        PORT4_EN_ON();
        __delay_ms(BLINK_DELAY_MS);

        // All OFF
        PORT1_EN_OFF();
        PORT2_EN_OFF();
        PORT3_EN_OFF();
        PORT4_EN_OFF();
        __delay_ms(BLINK_DELAY_MS);
    }
    //Then Turn On all
    PORT1_EN_ON();
    PORT2_EN_ON();
    PORT3_EN_ON();
    PORT4_EN_ON();
    __delay_ms(BLINK_DELAY_MS);
}

void set_ports_based_on_cc(void) {
    // Port 1
    if (!PORT1_CC_ATTACHED()) {
        PORT1_EN_OFF();
    }

    // Port 2
    if (!PORT2_CC_ATTACHED()) {
        PORT2_EN_OFF();
    }

    // Port 3
    if (!PORT3_CC_ATTACHED()) {
        PORT3_EN_OFF();
    }

    // Port 4
    if (!PORT4_CC_ATTACHED()) {
        PORT4_EN_OFF();
    }
}


// Initialize port states based on CC detection at boot
void initialize_ports(void) {
    // Port 1
    port_state[0].prev_cc_attached = PORT1_CC_ATTACHED();
    if (port_state[0].prev_cc_attached) {
        PORT1_EN_ON();
        port_state[0].enabled = true;
    } else {
        PORT1_EN_OFF();
        port_state[0].enabled = false;
    }

    // Port 2
    port_state[1].prev_cc_attached = PORT2_CC_ATTACHED();
    if (port_state[1].prev_cc_attached) {
        PORT2_EN_ON();
        port_state[1].enabled = true;
    } else {
        PORT2_EN_OFF();
        port_state[1].enabled = false;
    }

    // Port 3
    port_state[2].prev_cc_attached = PORT3_CC_ATTACHED();
    if (port_state[2].prev_cc_attached) {
        PORT3_EN_ON();
        port_state[2].enabled = true;
    } else {
        PORT3_EN_OFF();
        port_state[2].enabled = false;
    }

    // Port 4
    port_state[3].prev_cc_attached = PORT4_CC_ATTACHED();
    if (port_state[3].prev_cc_attached) {
        PORT4_EN_ON();
        port_state[3].enabled = true;
    } else {
        PORT4_EN_OFF();
        port_state[3].enabled = false;
    }
}


void main(void) {
    // Initialize everything from MCC (pins, clock, etc.)
    SYSTEM_Initialize();

     // Small delay for stability
    __delay_ms(100);

    // Step 1: Boot blink sequence (all ports flash 3 times)
    blink_all_three_times();

    // Step 2: After blinking, enable only ports with device attached (CC low)
    set_ports_based_on_cc();
    
    initialize_ports();
    
    // Persistence timers (ms) for each port
    //uint32_t low_timer[4] = {0};
    
    // Forever loop
     while (1) {
        // ========== PORT 1 ==========
        bool cc1_now = PORT1_CC_ATTACHED();
        
        // Detect attach event (was detached, now attached)
        if (!port_state[0].prev_cc_attached && cc1_now) {
            PORT1_EN_ON();
            port_state[0].enabled = true;
            port_state[0].low_timer_ms = 0;
        }
        // Detect detach event (was attached, now detached)
        else if (port_state[0].prev_cc_attached && !cc1_now) {
            PORT1_EN_OFF();
            port_state[0].enabled = false;
            port_state[0].low_timer_ms = 0;
        }
        port_state[0].prev_cc_attached = cc1_now;

        // Current monitoring (only if enabled)
        if (port_state[0].enabled) {
            float i1 = getCurrent_mA(CH_PORT1_CURR);
            if (i1 < LOW_CURRENT_MA) {
                port_state[0].low_timer_ms += 100;
                if (port_state[0].low_timer_ms >= PERSIST_MS) {
                    PORT1_EN_OFF();
                    port_state[0].enabled = false;
                    port_state[0].low_timer_ms = 0;
                }
            } else {
                port_state[0].low_timer_ms = 0;
            }
        }

        // ========== PORT 2 ==========
        bool cc2_now = PORT2_CC_ATTACHED();
        
        if (!port_state[1].prev_cc_attached && cc2_now) {
            PORT2_EN_ON();
            port_state[1].enabled = true;
            port_state[1].low_timer_ms = 0;
        }
        else if (port_state[1].prev_cc_attached && !cc2_now) {
            PORT2_EN_OFF();
            port_state[1].enabled = false;
            port_state[1].low_timer_ms = 0;
        }
        port_state[1].prev_cc_attached = cc2_now;

        if (port_state[1].enabled) {
            float i2 = getCurrent_mA(CH_PORT2_CURR);
            if (i2 < LOW_CURRENT_MA) {
                port_state[1].low_timer_ms += 100;
                if (port_state[1].low_timer_ms >= PERSIST_MS) {
                    PORT2_EN_OFF();
                    port_state[1].enabled = false;
                    port_state[1].low_timer_ms = 0;
                }
            } else {
                port_state[1].low_timer_ms = 0;
            }
        }

        // ========== PORT 3 ==========
        bool cc3_now = PORT3_CC_ATTACHED();
        
        if (!port_state[2].prev_cc_attached && cc3_now) {
            PORT3_EN_ON();
            port_state[2].enabled = true;
            port_state[2].low_timer_ms = 0;
        }
        else if (port_state[2].prev_cc_attached && !cc3_now) {
            PORT3_EN_OFF();
            port_state[2].enabled = false;
            port_state[2].low_timer_ms = 0;
        }
        port_state[2].prev_cc_attached = cc3_now;

        if (port_state[2].enabled) {
            float i3 = getCurrent_mA(CH_PORT3_CURR);
            if (i3 < LOW_CURRENT_MA) {
                port_state[2].low_timer_ms += 100;
                if (port_state[2].low_timer_ms >= PERSIST_MS) {
                    PORT3_EN_OFF();
                    port_state[2].enabled = false;
                    port_state[2].low_timer_ms = 0;
                }
            } else {
                port_state[2].low_timer_ms = 0;
            }
        }

        // ========== PORT 4 ==========
        bool cc4_now = PORT4_CC_ATTACHED();
        
        if (!port_state[3].prev_cc_attached && cc4_now) {
            PORT4_EN_ON();
            port_state[3].enabled = true;
            port_state[3].low_timer_ms = 0;
        }
        else if (port_state[3].prev_cc_attached && !cc4_now) {
            PORT4_EN_OFF();
            port_state[3].enabled = false;
            port_state[3].low_timer_ms = 0;
        }
        port_state[3].prev_cc_attached = cc4_now;

        if (port_state[3].enabled) {
            float i4 = getCurrent_mA(CH_PORT4_CURR);
            if (i4 < LOW_CURRENT_MA) {
                port_state[3].low_timer_ms += 100;
                if (port_state[3].low_timer_ms >= PERSIST_MS) {
                    PORT4_EN_OFF();
                    port_state[3].enabled = false;
                    port_state[3].low_timer_ms = 0;
                }
            } else {
                port_state[3].low_timer_ms = 0;
            }
        }

        __delay_ms(100);  // 100ms update interval
    }
}