        CFGVAR  "name" = "RAM 8 Narrow Test"
        CFGVAR  "short_name" = "RAM8 Test"
        CFGVAR  "author" = "Test"
        CFGVAR  "license" = "GPLv2+"

        ROMW    16

RAM8    EQU     $A000           ; RAM 8 test address (cfg: RAM 8)
BORDER  EQU     $2C             ; STIC border color
DISPEN  EQU     $20             ; STIC display enable

        ORG     $5000
ROMHDR: BIDECLE ZERO            ; MOB picture base
        BIDECLE ZERO            ; Process table
        BIDECLE MAIN            ; Program start address
        BIDECLE ZERO            ; Bkgnd picture base
        BIDECLE ONES            ; GRAM pictures
        BIDECLE TITLE           ; Cartridge title/date
        DECLE   $03C0           ; No ECS title, run code after title
ZERO:   DECLE   $0000
        DECLE   $0000
        DECLE   0, 0, 0, 0, 0
ONES:   DECLE   1
TITLE:  BYTE    101, "RAM8 Test", 0

;; Write $ABCD to 8-bit RAM, read back. Narrow RAM keeps only the low byte,
;; so the expected result is $00CD. Green border = pass, red = fail.
MAIN:   MVII    #$ABCD, R0
        MVO     R0, RAM8
        MVI     RAM8, R1
        MVII    #2, R0          ; red (fail)
        CMPI    #$00CD, R1
        BNE     SHOW
        MVII    #5, R0          ; green (pass)
SHOW:   MVO     R0, BORDER
        MVII    #1, R0
        MVO     R0, DISPEN
LOOP:   B       LOOP
