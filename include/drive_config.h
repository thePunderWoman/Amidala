// drive_config.h
// Drive and dome system configuration for Amidala Firmware.
//
// Edit the two "Active selection" lines to switch hardware configurations.
// Everything below the "Drive system resolution" comment is derived
// automatically — do not edit those lines.

#pragma once

// ---- Drive system type constants --------------------------------------------

#define DRIVE_SYSTEM_PWM                1
#define DRIVE_SYSTEM_SABER              2
#define DRIVE_SYSTEM_ROBOTEQ_PWM        3
#define DRIVE_SYSTEM_ROBOTEQ_SERIAL     4
#define DRIVE_SYSTEM_ROBOTEQ_PWM_SERIAL 5

// Active selection (uncomment the one in use):
// #define DRIVE_SYSTEM DRIVE_SYSTEM_ROBOTEQ_PWM_SERIAL
#define DRIVE_SYSTEM DRIVE_SYSTEM_ROBOTEQ_PWM

// ---- Dome drive type constants -----------------------------------------------

#define DOME_DRIVE_SABER    2
#define DOME_DRIVE_PWM      3
#define DOME_DRIVE_ROBOCLAW 4

// Active selection (uncomment the one in use):
// #define DOME_DRIVE DOME_DRIVE_PWM
// #define DOME_DRIVE DOME_DRIVE_SABER
#define DOME_DRIVE DOME_DRIVE_ROBOCLAW

// ---- Speed and acceleration parameters --------------------------------------

#define MAXIMUM_SPEED       1.0f  // percentage 0.0 - 1.0. default 50%
#define MAXIMUM_GUEST_SPEED 0.5f  // percentage 0.0 - 1.0. default 30%
#define DOME_MAXIMUM_SPEED  1.0f  // percentage 0.0 - 1.0. default 100%

// Scale value of 1 means instant. Scale value of 100 means that the throttle
// will increase 1/100 every 25ms
#define ACCELERATION_SCALE 100
// Scale value of 1 means instant. Scale value of 20 means that the throttle
// will decrease 1/20 every 25ms
#define DECELRATION_SCALE  20

// Set to true if acceleration/deceleration should be applied
#define SCALING false

// ---- Dome motion defaults ---------------------------------------------------

#define DEFAULT_DOME_HOME_POSITION      50
#define DEFAULT_DOME_ACCELERATION_SCALE 20
#define DEFAULT_DOME_DECELERATION_SCALE 50
#define DEFAULT_DOME_SEEK_MIN_DELAY     6
#define DEFAULT_DOME_SEEK_MAX_DELAY     8
#define DEFAULT_DOME_TARGET_MIN_DELAY   0
#define DEFAULT_DOME_TARGET_MAX_DELAY   1
#define DEFAULT_DOME_SEEK_LEFT          80
#define DEFAULT_DOME_SEEK_RIGHT         80
#define DEFAULT_DOME_FUDGE              10
#define DEFAULT_DOME_SPEED_HOME         40
#define DEFAULT_DOME_SPEED_SEEK         60
#define DEFAULT_DOME_SPEED_TARGET       100
#define DEFAULT_DOME_SPEED_MIN          5
#define DEFAULT_DOME_DECEL_ZONE         30
#define DEFAULT_DOME_TIMEOUT            5
#define DEFAULT_DOME_INVERTED           true
#define DEFAULT_DOME_MIN_PULSE          1000
#define DEFAULT_DOME_MAX_PULSE          2000

// ---- RoboClaw dome drive defaults -------------------------------------------
// These apply only when DOME_DRIVE == DOME_DRIVE_ROBOCLAW.

// Serial port used by the RoboClaw.
// UART1 (GPIO17/18) — Serial0 (UART0) carries the WCB downstream output.
// DOME_DRIVE_SERIAL is intentionally NOT defined here so pin_config.h still
// assigns SERIAL = Serial0 for the primary output.
#if DOME_DRIVE == DOME_DRIVE_ROBOCLAW
#define ROBOCLAW_SERIAL    Serial1   // UART1, GPIO17/18
#define ROBOCLAW_RX_PIN    18        // UART1 RX
#define ROBOCLAW_TX_PIN    17        // UART1 TX
#define ROBOCLAW_BAUD_RATE 38400
#endif

// Default RoboClaw hardware settings
#define DEFAULT_DOME_ROBOCLAW_ADDRESS 128  // packet-serial default address (0x80)
#define DEFAULT_DOME_ROBOCLAW_CHANNEL 1    // motor channel: 1 = M1, 2 = M2

// Encoder counts per motor shaft revolution (CPR).
// For the Pololu motor in this build the CPR is 1200.
// Change this constant if a different motor is swapped in; it is not a
// runtime config key because it is a hardware property of the motor itself.
#define DOME_MOTOR_CPR 1200

// Max encoder pulses per second sent to RoboClaw SpeedM1/M2.
// Tune this to the motor's realistic no-load maximum before applying load.
#define DEFAULT_DOME_ROBOCLAW_QPPS 800

// Milliseconds of zero encoder movement while the motor is commanded before
// the system declares an obstruction and stops.
#define DEFAULT_DOME_STALL_TIMEOUT_MS 500

// Milliseconds allowed for the homing sweep to find the hall sensor.
// If it expires the system falls back to manual mode with a console warning.
#define DEFAULT_DOME_HOMING_TIMEOUT_MS 15000

// Speed (fraction of QPPS) used during automatic homing and calibration.
// Keep these low enough to be safe on first power-on.
#define DEFAULT_DOME_HOMING_SPEED   0.20f  // 20 % of QPPS
#define DEFAULT_DOME_CALIBRATE_SPEED 0.30f // 30 % of QPPS

// Hardware debounce window for the hall-effect sensor interrupt (ms).
// Triggers arriving faster than this are ignored.
#define DEFAULT_DOME_HALL_DEBOUNCE_MS 50

// Number of complete dome revolutions averaged during calibration.
#define DOME_CALIBRATION_ROTATIONS 10

// Sequence-pause watchdog (dome=seqon[,<seconds>]).  If no dome=seqoff arrives
// within the pause window, the pause auto-clears so the dome resumes its
// normal auto-behaviors even if the body/dome controller crashes mid-sequence.
#define DEFAULT_DOME_SEQUENCE_PAUSE_MS 30000   // 30s default when no arg given
#define DOME_SEQUENCE_PAUSE_MAX_MS     300000  // 5 min safety cap

// EEPROM layout for RoboClaw calibration data.
// Stored at a fixed offset well past the existing SC23 XBee block (0x64)
// and servo block (0xEA) to avoid collisions.
//
//   Offset  Size  Contents
//   0x200   4     Signature bytes: 'R','C','0','1'
//   0x204   4     int32_t  ticksPerDomeRev  (0 → not yet calibrated)
//
// Reading: if the 4-byte signature matches, load ticksPerDomeRev.
// Writing: always write both signature and data atomically.
#define DOME_ROBOCLAW_EEPROM_ADDR 0x200
#define DOME_ROBOCLAW_EEPROM_SIG0 'R'
#define DOME_ROBOCLAW_EEPROM_SIG1 'C'
#define DOME_ROBOCLAW_EEPROM_SIG2 '0'
#define DOME_ROBOCLAW_EEPROM_SIG3 '1'

// ---- Drive system resolution (derived — do not edit) ------------------------

#if DRIVE_SYSTEM == DRIVE_SYSTEM_SABER
#define DRIVE_SERIAL      Serial0   // UART0, GPIO43/44
#define DOME_DRIVE_SERIAL DRIVE_SERIAL
#define CHANNEL_MIXING    true  // premix channels before sending commands
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_PWM
#define CHANNEL_MIXING    false // motor controller will mix the channels
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM
#define CHANNEL_MIXING    false // motor controller will mix the channels
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_SERIAL
#define DRIVE_BAUD_RATE   115200
#define CHANNEL_MIXING    false // motor controller will mix the channels
#elif DRIVE_SYSTEM == DRIVE_SYSTEM_ROBOTEQ_PWM_SERIAL
#define DRIVE_BAUD_RATE   115200
#define CHANNEL_MIXING    false // motor controller will mix the channels
#else
#error Unsupported DRIVE_SYSTEM
#endif

// #define EXPERIMENTAL_JEVOIS_STEERING

#if !defined(DRIVE_BAUD_RATE) && DOME_DRIVE == DOME_DRIVE_SABER
#define DRIVE_BAUD_RATE 9600
#endif

#if !defined(DOME_DRIVE_SERIAL) && DOME_DRIVE == DOME_DRIVE_SABER
#define DOME_DRIVE_SERIAL Serial0
#endif

#if defined(DOME_DRIVE) && DOME_DRIVE != DOME_DRIVE_SABER && \
    DOME_DRIVE != DOME_DRIVE_PWM && DOME_DRIVE != DOME_DRIVE_ROBOCLAW
#error Unsupported DOME_DRIVE
#endif
