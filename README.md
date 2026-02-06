```
                    Volume Zero, Issue 3, Phile 1 of 1

                          M5PORKCHOP README
                          v0.1.8a-heapkampf

                            ^__^
                            (oo)\_______
                            (__)\       )\/\
                                ||----w |
                                ||     ||
                (yes that's a cow. the pig ate the pig art budget.)
                (the horse was unavailable for comment.)


                67% of skids skip READMEs.
                100% of those skids report bugs we documented.
                the horse thinks this is valid.


                         TABLE OF CONTENTS

    1. WHAT THE HELL IS THIS
    2. MODES (what the pig does)
    3. THE PIGLET (mood, avatar, weather)
    4. THE FORBIDDEN CHEESE ECONOMY (XP, ranks, trophies)
    5. CLOUD HOOKUPS (WiGLE / WPA-SEC)
    6. PIGSYNC (son of a pig)
    7. THE MENUS
    8. CONTROLS
    9. SD CARD LAYOUT
    10. BUILDING
    11. LEGAL
    12. TROUBLESHOOTING (the confessional)
    13. GREETZ

    (pro tip: CTRL+F "horse" for enlightenment.
     we counted them. the horse counted more.
     one of us is wrong. both of us are the barn.)

```
```
+==============================================================+
|                                                              |
| [O] OINK MODE     - yoink handshakes. question ethics.       |
| [D] DO NO HAM     - zero TX. passive recon. zen pig.         |
| [W] WARHOG        - GPS wardriving. legs required.           |
| [H] SPECTRUM      - RF analysis. client hunting. fangs.      |
| [B] PIGGY BLUES   - BLE spam. YOU DIED. in that order.       |
| [A] BACON         - fake beacons. confuse everything.        |
| [F] FILE XFER     - web UI. civilization achieved.           |
|                                                              |
| [1] PIG DEMANDS   - session challenges. three trials.        |
| [2] PIGSYNC       - the prodigal son answers the phone.      |
|                                                              |
| REMEMBER:                                                    |
| 1. only attack what you own or have written permission to    |
| 2. "because it can" is not a legal defense                   |
| 3. the pig watches. the law judges. know the difference.     |
|                                                              |
+==============================================================+
```
```

--[ 1 - WHAT THE HELL IS THIS

    three questions every operator asks:
    1. "what does it do?"
    2. "is it legal?"
    3. "why does the pig look disappointed in me?"


----[ 1.1 - THE ELEVATOR PITCH

    PORKCHOP runs on M5Cardputer (ESP32-S3, 240MHz, 8MB flash).
    it turns a pocket keyboard into a WiFi pentesting companion with:

    - promiscuous mode packet capture and EAPOL extraction
    - GPS wardriving with WiGLE v1.6 export
    - 2.4GHz spectrum analysis with client tracking
    - BLE notification spam (Apple/Android/Samsung/Windows)
    - beacon injection with vendor IE fingerprinting
    - device-to-device sync via ESP-NOW (PigSync)
    - a personality system with opinions about your choices

    it's a learning tool for WiFi security research.
    the difference between tool and weapon is the hand holding it.


----[ 1.2 - THE LINEUP (what you're getting into)

    words are cheap. pixels are evidence.

    THE PIG IN THE FLESH:

    ![PORKCHOP on Cardputer ADV - IDLE screen](docs/images/porkchop_idle.jpg)
    *the pig. awake. judging. 240x135 pixels of unsolicited opinions.*

    ![OINK MODE - handshake capture in progress](docs/images/porkchop_oink.jpg)
    *oink mode. the pig is working. you should be concerned.*

    ![SPECTRUM - 2.4GHz visualization with client monitor](docs/images/porkchop_spectrum.jpg)
    *spectrum analysis. gaussian lobes. the pig does math now.*

    ![SGT WARHOG - GPS wardriving session](docs/images/porkchop_warhog.jpg)
    *sgt warhog reporting for duty. GPS locked. networks catalogued.*

    ![PIGGY BLUES - BLE chaos selection](docs/images/porkchop_piggyblues.jpg)
    *piggy blues. the screen before regret. the calm before the storm.*

    ![PORKCHOP COMMANDER - web file manager](docs/images/porkchop_commander.jpg)
    *porkchop commander. the pig serves HTTP. we've come so far.*

    THE HARDWARE NAKED:

    ![Cardputer ADV with CapLoRa module](docs/images/hardware_cardputer_adv.jpg)
    *cardputer ADV + CapLoRa868. GPS + LoRa on a keyboard.*
    *smaller than your phone. more opinions than your family.*

    ![The full kit - Cardputer + CapLoRa + GPS antenna](docs/images/hardware_full_kit.jpg)
    *the full loadout. everything the pig needs to judge your neighborhood.*

    (photos missing? we're working on it. the pig doesn't pose for free.
     the horse refused the photoshoot entirely. barn lighting was wrong.)


----[ 1.3 - HARDWARE SPECS

    M5Cardputer (M5Stack StampS3):
        (EAPOL has M1, M2, M3, M4. the hardware is M5.
         the fifth message. the one the protocol never sent.
         we are the frame after the handshake.)
        - ESP32-S3FN8: 240MHz dual-core, 512KB SRAM, 8MB flash
        - NO PSRAM (~300KB usable heap. every byte matters.)
        - 240x135 IPS LCD (ST7789V2)
        - QWERTY keyboard (56 keys)
        - SD card slot (FAT32, shared SPI bus with CapLoRa)
        - NeoPixel LED (GPIO 21)
        - USB-C (power + serial)

    Optional:
        - AT6558 GPS module (Grove port G1/G2)
        - CapLoRa868 module (G15/G13 GPS, SX1262 LoRa shares SD SPI)
        - Cardputer ADV: BMI270 IMU enables dial mode in Spectrum
        - your legs, for wardriving. no judgment on wheels.

    GET THE HARDWARE (the part where we sell out):

        look. we're going to be transparent here.
        these are affiliate links. we get a cut.
        you get a pig. capitalism works (sometimes).

        the pig doesn't mass-produce itself.
        the pig needs a body. the body needs a store.
        the store needs a link. the link needs a click.
        you see where this is going.

        M5Stack Cardputer ADV (the pig's body):
        https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3/?ref=xqezhcga

        CapLoRa 1262 (GPS + LoRa module, the pig's sense of direction):
        https://shop.m5stack.com/products/cap-lora-1262-for-cardputer-adv-sx1262-atgm336h

        buying through these links funds:
        1. coffee (which becomes code)
        2. code (which becomes bugs)
        3. bugs (which become trauma)
        4. trauma (which becomes coffee)
        5. the circle continues. you're an investor now.

        not buying? also valid. the pig judges purchases,
        not people. clone it, build it, source it however.
        open source means open source.

        but if you DO click... the pig remembers its friends.


----[ 1.4 - ARCHITECTURE (for the silicon gourmets)

    THE CORE:
    cooperative main loop. porkchop.update(), Display::update(),
    SFX::update(). three calls. twenty thousand lines behind them.
    single PorkchopMode enum, 23 states. one mode lives. the others wait.

    THE MEMORY WAR:
    no PSRAM means ~300KB internal SRAM for everything.
    TLS needs ~35KB contiguous (kMinContigForTls in heap_policy.h).
    WiFi driver init needs ~70KB (why preInitWiFiDriverEarly() exists).
    the heap fragments like your relationship with sleep.

    HEAP CONDITIONING:
    boot runs a 5-phase ritual: frag blocks (50x1KB), struct blocks
    (20x3KB), TLS test allocs (26KB, 32KB, 40KB). exploits ESP-IDF's
    TLSF allocator for O(1) coalescing. the result: a clean brain
    for TLS handshakes. percussive maintenance for memory.

    HEAP MONITORING:
    heap_health.h samples every 1s. auto-triggers conditioning at
    65% fragmentation, clears at 75%. the heart bar at the bottom
    of the screen is heap health. 100% = clean. 0% = swiss cheese.

    THE EVENT BUS:
    max 32 queued events, 16 processed per update tick.
    HANDSHAKE_CAPTURED, NETWORK_FOUND, GPS_FIX, GPS_LOST,
    DEAUTH_SENT, ROGUE_AP_DETECTED, MODE_CHANGE, LOW_BATTERY.
    the pig processes feelings through a state machine.

    DUAL-CORE PATTERN:
    WiFi promiscuous callbacks run on core 1 (WiFi task).
    they CANNOT allocate memory or call Serial. instead:
    callback sets volatile flags + writes to static pools.
    main loop on core 0 checks flags and processes safely.
    this is the deferred event pattern. it keeps the WDT happy.

    STATIC POOLS:
    OINK pre-allocates PendingHandshakeFrame pendingHsPool[4]
    (~13KB in BSS). permanent. std::atomic indices for lock-free
    producer/consumer between WiFi task and main loop.

    NetworkRecon:
    shared background scanning service. OINK, DONOHAM, SPECTRUM,
    and WARHOG all consume the same getNetworks() vector.
    spinlock mutex with RAII CriticalSection wrapper.
    channel hop order: 1, 6, 11, then 2-5, 7-10, 12-13.
    max 200 networks tracked. the pig has limits.

    BOOT GUARD:
    RTC memory tracks rapid reboots. 3 in 60 seconds = force IDLE.
    crash loops get the nuclear option. the pig learns from pain.

------------------------------------------------------------------------

--[ 2 - MODES (what the pig does)

    one keypress from IDLE. zero menus. zero friction.
    each mode changes the pig's vocabulary.
    this is not a bug. this is character development.


----[ 2.1 - OINK MODE (active hunt) [O]

    the pig goes rowdy. opinions about APs intensify.
    something about "sorting things out."

    CAPABILITIES:
        - channel hop across 2.4GHz (13 channels, adaptive timing)
        - promiscuous mode 802.11 frame capture
        - EAPOL handshake extraction (M1-M4, validates sequence)
        - PMKID extraction from RSN IE in M1 frames
          (the AP just volunteers it. trust issues are real.)
        - deauth (Reason 7) + disassoc (Reason 8) with jitter
          (1-5ms random between frames. less predictable to WIDS.)
        - PMF detection: Protected Management Frames networks
          get marked immune. the pig respects armor.
        - targeted client deauth (up to 20 clients per network)
        - broadcast disassoc alongside broadcast deauth
          (some devices ignore deauth but fold to disassoc)
        - hashcat 22000 export (.22000 files, zero preprocessing)
        - PCAP export with radiotap headers (Wireshark-ready)
        - auto-cooldown on targets (no spam, surgical precision)
        - smart target selection (priority scoring: RSSI, activity,
          client count, beacon stability)

    LOCK-ON DISPLAY:
        target SSID (or <HIDDEN>), client count,
        capture progress (M1/M2/M3/M4 indicators),
        quality score, beacon interval EMA.

    BOAR BROS: press [B] to whitelist a network.
        bros get observed, not attacked. the pig has honor.
        manage in LOOT > BOAR BROS. max 50 networks.
        zero heap allocation (fixed array in BSS).

    SEAMLESS TOGGLE: press [D] to flip into DO NO HAM.
        same radio state. different conscience.


----[ 2.2 - DO NO HAM (passive recon) [D]

    the pig goes zen. zero transmissions. pure observation.
    the ether provides to those who wait.

    CAPABILITIES:
        - promiscuous receive ONLY (zero TX, zero deauth)
        - adaptive channel timing with state machine:
            HOPPING    = scanning all channels
            DWELLING   = found activity, staying for SSID backfill
            HUNTING    = partial EAPOL detected, extended dwell
            IDLE_SWEEP = dead channels, fast sweep
        - channel stats track beacons/EAPOL per channel
          primary channels (1, 6, 11) get longer dwell times
        - passive PMKID catches (APs volunteer PMKIDs in M1 frames)
        - passive handshake capture from natural reconnects
        - incomplete handshake tracking (shows missing frames,
          max 20 tracked, 60s age-out, automatic revisit)
        - DNH_PMKID_GHOST achievement: 150 XP for passive PMKID
          (the rarest catch. the pig respects patience.)

    press [D] again to flip back to OINK.


----[ 2.3 - SGT WARHOG (wardriving) [W]

    the pig goes tactical. salutes things.
    starts talking about sectors and objectives.

    CAPABILITIES:
        - continuous AP scanning with GPS correlation
        - WiGLE CSV v1.6 export (WigleWifi-1.6 format)
        - internal CSV with extended fields
        - dedup bloom filter (no duplicate entries)
        - distance tracking for XP (DISTANCE_KM = +30 XP)
        - capture marking for bounty system
        - file rotation for session management
        - direct-to-disk writes (no entries[] accumulation)

    BOUNTY SYSTEM: wardriven networks become targets for
    PigSync. your Sirloin companion can hunt what you found.
    max 15 bounty BSSIDs per sync payload.
    manage in LOOT > BOUNTIES.

    FILES: /m5porkchop/wardriving/*.wigle.csv

    no GPS? no problem. pig logs, just zero coordinates.
    but c'mon. get GPS. the pig wants closure.


----[ 2.4 - HOG ON SPECTRUM (RF analysis) [H]

    pure concentration. the pig does math now.

    CAPABILITIES:
        - 2.4GHz spectrum visualization (channels 1-14)
        - gaussian lobes per network (RSSI = height)
        - noise floor animation at baseline
        - waterfall display (historical spectrum, push per update)
        - VULN indicator (WEP/OPEN networks. in 2025+. respect.)
        - BRO indicator (networks in boar_bros.txt)
        - filter modes: ALL / VULN / SOFT (no PMF) / HIDDEN
        - render snapshot buffer (64 networks, heap-safe)
        - packet-per-second counter (callback-safe atomic)

    DIAL MODE (Cardputer ADV only):
        BMI270 IMU detected = tilt-to-tune channel selection.
        hold device upright, tilt to pan channels 1-13.
        lerped smooth display. hysteresis for FLT/UPS detection.
        [SPACE] toggles channel lock in dial mode.
        (original Cardputer lacks IMU. dial mode auto-disables.)

    CLIENT MONITOR (press [ENTER] on selected network):
        - channel locks. the hunt begins.
        - connected clients: MAC, vendor (OUI, 450+ entries),
          RSSI, freshness timer, proximity arrows
        - proximity arrows:
            >> = much closer to you than the AP (+10dB)
            >  = closer (+3 to +10dB)
            == = roughly equal distance
            <  = farther (-3 to -10dB)
            << = much farther (-10dB)
        - [ENTER] on client = deauth burst (5 frames each way)
        - [W] = REVEAL MODE: broadcast deauth to flush hiding
          clients out of association. periodic bursts.
        - client detail popup (vendor, signal, timestamps)
        - max 8 clients tracked, 4 visible on screen
        - stale timeout: 30s. signal lost: 15s = auto-exit.
        - achievement hunting: QU1CK DR4W, D34D 3Y3, H1GH N00N

    CONTROLS:
        [,] [/]     pan frequency view left/right
        [;] [.]     cycle selected network
        [F]         cycle filter (ALL/VULN/SOFT/HIDDEN)
        [ENTER]     enter client monitor
        [SPACE]     toggle dial lock (in dial mode)

    marco polo, but with packets.


----[ 2.5 - PIGGY BLUES (BLE chaos) [B]

    something theatrical awakens. the pig suggests
    you reconsider. you don't. nobody does.

    CAPABILITIES:
        - vendor-aware BLE spam targeting:
            Apple    - AirDrop proximity popups
            Android  - Fast Pair notifications
            Samsung  - Galaxy ecosystem triggers
            Windows  - Swift Pair dialogs
        - continuous NimBLE passive scan + opportunistic advertising
        - no-reboot roulette counter (for achievement hunters)
        - vendor identification from manufacturer data

    WARNING DIALOG ON FIRST ENTRY: [Y] to confirm.
    "NO LOLLYGAGGIN'" - the stack commands.

    EXIT RITUAL: leave PIGGY BLUES and face judgment.
    YOU DIED. five seconds of reckoning. then rebirth.
    or the stack decides otherwise.

    NimBLE: internal RAM only (CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_INTERNAL=1).
    BLE deinit reclaims 20-30KB. the pig breathes again.

    this mode is LOUD. everyone in BLE range knows.
    don't do this in public. don't. (the pig warned you.)


----[ 2.6 - BACON MODE (beacon spam) [A]

    the pig becomes a beacon factory on channel 6.

    CAPABILITIES:
        - fake AP beacon transmission with vendor IE fingerprinting
          (OUI: 0x50:52:4B = "PRK". we branded the chaos.)
        - AP count camouflage (broadcasts top 3 nearby APs in IE)
        - three TX tiers: [1] Fast 50ms, [2] Balanced 100ms,
          [3] Slow 150ms. plus random jitter (0-50ms).
        - sequence number tracking. session time counter.
        - beacons-per-second rate display.

    USE CASE: confuse WiFi scanners in YOUR test environment.
    "YOUR test environment" means YOUR lab, YOUR network,
    YOUR understanding that this is for research.
    not "that conference." not "your office." your lab.


----[ 2.7 - PORKCHOP COMMANDER (file transfer) [F]

    the pig transcends firmware. the pig serves HTTP.

    CAPABILITIES:
        - connects to configured WiFi (settings > WiFi)
        - serves browser UI for SD file management
        - mDNS: porkchop.local (type it in your browser)
        - dual-pane norton commander layout (tab between panes)
        - upload/download/delete/rename/move/copy
        - multi-select with space, bulk operations
        - keyboard navigation with F-key bar
        - SWINE summary endpoint (XP/stats JSON)
        - session transfer stats (bytes in/out)

    HEAP-AWARE: large transfers may queue or reject if memory
    is tight. file server needs 40KB free + 30KB largest block
    (kFileServerMinHeap, kFileServerMinLargest in heap_policy.h).

    exfil YOUR OWN captures without pulling the SD card.
    like a civilized person. with a browser.


----[ 2.8 - CHARGING MODE (low power)

    plug in USB. the pig rests.

    auto-enters when external power detected and battery low.
    shows battery percent, voltage, charge rate estimate,
    minutes-to-full calculation from voltage history.
    suspends NetworkRecon and GPS to minimize draw.
    auto-exits when power removed.

    the pig has better battery management than most phones.

------------------------------------------------------------------------

--[ 3 - THE PIGLET (mood, avatar, weather)

    the pig has feelings. deterministic, simulated feelings.
    we commit to the bit. shipped anyway.


----[ 3.1 - MOOD SYSTEM

    happiness score: -100 to +100

    POSITIVE TRIGGERS:
        captures (pig happy), new networks (pig curious),
        GPS fix (pig validated), distance logged (pig active)

    NEGATIVE TRIGGERS:
        idle time (pig bored), low battery (pig annoyed),
        GPS loss (pig betrayed), prolonged inactivity (pig sad)

    MOMENTUM:
        short-term mood modifier. 30-second decay window.
        recent captures boost momentum. idle drains it.
        momentum feeds into mood buffs (see section 4).

    the pig remembers session performance.
    the pig is an emotional state machine with side effects.


----[ 3.2 - AVATAR

    ASCII pig with animation system:
        - 7 states: NEUTRAL, HAPPY, EXCITED, HUNTING, SLEEPY, SAD, ANGRY
        - blink animation (intensity-modulated by mood)
        - ear wiggle, nose sniff, cute jump on captures
        - directional facing (left/right)
        - walk transitions (smooth slide across screen)
        - attack shake (visual feedback on captures)
        - grass animation at bottom (scrolling binary pattern,
          direction tracks pig movement, configurable speed)

    NIGHT MODE (20:00-06:00 local time):
        15 stars appear with fade-in effects. twinkling.
        RTC-governed vigil. the pig respects circadian rhythms.
        the pig has better sleep hygiene than you.


----[ 3.3 - WEATHER SYSTEM

    mood-reactive atmospheric effects on screen:

    CLEAR        = high momentum, good session
    CLOUDY       = neutral, clouds drift in parallax
    RAIN         = low momentum, particle rain
    THUNDERSTORM = rage build, lightning flash inverts screen
    WIND         = directional particles in gusts

    mood level drives storm probability.
    the pig's emotional state affects the entire display.

    why? someone said "what if the pig had weather?" at 2am.
    nobody stopped us. weather shipped.


----[ 3.4 - PIG HEALTH (HEAP BAR)

    the heart bar at the bottom = heap health.
    100% = defragmented, contiguous blocks available.
    0%   = fragmented beyond use. TLS will fail.

    WHY IT FLUCTUATES:
    - wifi driver reallocations, file server, mode transitions

    HOW TO HEAL:
    - run OINK (channel hopping forces wifi buffer cleanup)
    - reboot (nuclear option, always valid)

    low health = cloud uploads fail. the pig needs a clean
    brain to negotiate TLS with WiGLE or WPA-SEC.

------------------------------------------------------------------------

--[ 4 - THE FORBIDDEN CHEESE ECONOMY

    the system tracks progress. the system remembers.

    we removed the full documentation for this section.
    knowing the algorithm changes the behavior.
    mystery is a feature. you are the rat. find the cheese.

    (do not put cheese in the SD slot. someone tried.)


----[ 4.1 - THE LADDER

    50 levels. 10 class tiers. every 5 levels, a new name.

    L01-05  SH0AT           fresh firmware, finding bearings
    L06-10  SN1FF3R         packet sniffer, learning the ether
    L11-15  PWNER           first real exploits, growing teeth
    L16-20  R00T            root-level awareness, the pig approves
    L21-25  R0GU3           handshake hunter, surgical precision
    L26-30  EXPL01T         man-in-the-middle boar mentality
    L31-35  WARL0RD         commanding the airwaves
    L36-40  L3G3ND          PMF-savvy, feared in the spectrum
    L41-45  K3RN3L_H0G      multi-link legend, nearing ascension
    L46-50  B4C0NM4NC3R     endgame myth. the pig bows to none.

    TITLE OVERRIDES: special playstyle-based titles unlock:
        SH4D0W_H4M    - 500 passive networks (shadow broker)
        P4C1F1ST_P0RK - 25 boar bros (witness protection)
        Z3N_M4ST3R    - 5 passive PMKIDs (the rarest path)

    CALLSIGN: custom handle unlocks at L10.


----[ 4.2 - XP EVENTS (abbreviated)

    NETWORK_FOUND +1 | HIDDEN +3 | WPA3 +10 | OPEN +3 | WEP +5
    HANDSHAKE +50 | PMKID +75 | DNH_PMKID_GHOST +150
    DEAUTH_SENT +1 | DEAUTH_SUCCESS +15
    DISTANCE_KM +30 | GPS_LOCK +5 | SMOKED_BACON +15
    SESSION_30MIN +10 | SESSION_60MIN +25 | SESSION_120MIN +50
    LOW_BATTERY_CAPTURE +20 (clutch plays rewarded)
    BOAR_BRO_ADDED +5 | BOAR_BRO_MERCY +15

    the exact values are in xp.h. read the source.
    or don't. the cheese finds the worthy.


----[ 4.3 - TROPHIES (64 achievements)

    64 achievements packed into a uint64_t bitfield.
    efficient and unhinged.

    some highlights:
        FIRST_BLOOD       - first handshake (everyone remembers)
        CENTURION         - 100 networks in one session
        MARATHON_PIG      - 10km walked in a session
        GHOST_HUNTER      - 10 hidden networks
        SILICON_PSYCHO    - 5,000 lifetime networks
        NIETZSWINE        - stare at spectrum for 15 minutes
        OINKAGEDDON       - 10,000 BLE packets sent
        ULTRAMARATHON     - 42.195km in one session (actual marathon)
        ABOUT_JUNKIE      - press Enter 5x in About screen
        PROPHECY_WITNESS  - witnessed the riddle prophecy
        FULL_CLEAR        - all other achievements unlocked

    some unlock title overrides. some require suffering.
    how do you get them? play. observe patterns. the pig drops hints.

    check LOOT > ACHIEVEMENTS. fill the grid.


----[ 4.4 - PIG DEMANDS (session challenges)

    three trials per session: EASY, MEDIUM, HARD.
    generated fresh each boot from 12 challenge types:

    NETWORKS_FOUND, HIDDEN_FOUND, HANDSHAKES, PMKIDS,
    DEAUTHS, GPS_NETWORKS, BLE_PACKETS, PASSIVE_NETWORKS,
    NO_DEAUTH_STREAK, DISTANCE_M, WPA3_FOUND, OPEN_FOUND

    XP scales: EASY = base, MEDIUM = 2x, HARD = 4x.
    conditional challenges fail if you violate the constraint
    (deauth invalidates a passive streak).

    press [1] from IDLE to see current demands.
    the pig has expectations.


----[ 4.5 - PERSISTENCE

    XP lives in NVS (flash). survives reboots and firmware updates.

    SD BACKUP: /m5porkchop/xp/xp_backup.bin
    auto-written on every save. M5Burner nukes NVS?
    pig recovers from SD on boot. the pig is immortal.

    DEVICE-BOUND + SIGNED:
    you can't copy it. you can't hex-edit it.
    tamper = LV1 SH0AT. the pig knows.

    earn your rank. or crack the signature.
    either way, you've learned something.

    UNLOCKABLES: secret challenges (bitfield in xp_backup).
    the baud rate is not just for modems.
    115200. 8N1. pig endures.

------------------------------------------------------------------------

--[ 5 - CLOUD HOOKUPS (WiGLE / WPA-SEC)

    the pig talks to the internet. uploads research.
    downloads validation. this is called "integration."


----[ 5.1 - WiGLE

    competitive leaderboard for wardrivers.

    SETUP (one time):
        1. create /m5porkchop/wigle/wigle_key.txt
        2. contents: apiname:apitoken (from wigle.net/account)
        3. key file auto-deletes after import (security)

    FEATURES:
        - upload .wigle.csv wardriving files
        - download user stats (rank, discoveries, distance)
        - upload tracking (no re-uploads)
        - XP award per upload (one-time per file)
        - WiGLE stats tab in SWINE STATS ([<] [>] to navigate)
        - extended timeout windows (WiGLE is slow. we're patient.)

    press [S] in WiGLE menu to sync.


----[ 5.2 - WPA-SEC

    distributed handshake cracking. their GPUs. your captures.

    SETUP:
        1. create /m5porkchop/wpa-sec/wpasec_key.txt
        2. contents: 32-char hex key from wpa-sec.stanev.org
        3. key file auto-deletes after import

    FEATURES:
        - upload .22000 and .pcap captures
        - download potfile (cracked results)
        - per-capture crack status indicators
        - ALREADY UPLOADED tracking (no phantom submissions)
        - XP award for submissions

    press [S] in LOOT > CAPTURES to sync.


----[ 5.3 - IF UPLOADS FAIL

    TLS needs ~35KB contiguous heap.

    1. stop promiscuous mode (exit OINK/DNH)
    2. check DIAGNOSTICS for heap status
    3. let heap_health auto-condition (wait 30s)
    4. try again

    HeapGates::checkTlsGates() runs before every upload.
    if it says no, the pig can't negotiate TLS.
    ESP32 gonna ESP32.

------------------------------------------------------------------------

--[ 6 - PIGSYNC (son of a pig)

    ESP-NOW encrypted sync between POPS (Porkchop) and
    SON (Sirloin). the prodigal son answers the phone.


----[ 6.1 - THE PROTOCOL

    press [2] from IDLE to enter PigSync.

    DISCOVERY: broadcast on channel 1. Sirloin responds
    with MAC, pending capture count, flags, RSSI.

    CONNECTION: CMD_HELLO -> RSP_HELLO (session token +
    data channel assignment). channels 3/4/8/9/13 selected
    to avoid congested 1/6/11.

    DATA TRANSFER: chunked, 238 bytes per fragment.
    sequence numbers, CRC32 verification, ACK per chunk.
    5 retries per chunk. 60 second transfer timeout.

    ENCRYPTION: ESP-NOW encrypted unicast.
    PMK: "SONOFAPIGKEY2024" (set once at init)
    LMK: "PORKCHOPSIRLOIN!" (set per-peer)
    (both sides must match. source is truth.)

    BOUNTIES: wardriven network BSSIDs sent as hunting
    targets. max 15 per payload. Sirloin reports matches.

    TIME SYNC: Sirloin has RTC, Porkchop doesn't.
    CMD_TIME_SYNC with RTT calculation for accuracy.


----[ 6.2 - THE DIALOGUE

    PigSync has a conversation system. three tracks.

    Papa greets:  "ABOUT TIME YOU SHOWED UP"
    Son replies:  "PAPA ITS YOUR FAVORITE MISTAKE"

    goodbye tiers based on capture count:
    0 captures: "EMPTY HANDED AGAIN"
    10+:        "HASHCAT GONNA EAT GOOD"

    roasts for failure:
    "ZERO PMKIDS? NOT MY SON"
    "SEGFAULT IN MY FEELINGS"
    "SKILL ISSUE I KNOW"

    SYNC HINTS during transfer:
    "praise the bandwidth" / "git gud at waiting"

    the family dynamics are... complicated.
    but the protocol works. captures transfer. CRC validates.
    reconciliation through packet delivery.


----[ 6.3 - BEACON GRUNTS (Phase 3)

    Sirloin broadcasts BeaconGrunt packets when idle:
    MAC, capture count, battery, storage, mood tier,
    RTC time, uptime, short name.

    connectionless passive awareness. Porkchop can see
    Sirloin's status without initiating a call.

    the son is out there. broadcasting.
    porkchop listens. porkchop hopes.

------------------------------------------------------------------------

--[ 7 - THE MENUS

    press [ESC] or navigate via bottom bar.
    we don't know when to stop. the pig doesn't either.


----[ 7.1 - MAIN MENU (from IDLE)

    LOOT:
        CAPTURES     - handshakes + PMKIDs with WPA-SEC status
        WIGLE        - wardriving files with WiGLE status
        ACHIEVEMENTS - trophy grid (64 slots)
        BOAR BROS    - whitelist management (BSSID + SSID)
        BOUNTIES     - unclaimed wardriven network targets

    STATS:
        SWINE STATS  - three tabs: STATS / BUFFS / WIGLE
                       lifetime networks, handshakes, PMKIDs,
                       deauths, distance, session time, streak,
                       active buffs, class perks, WiGLE rank

    SETTINGS:
        Personality  - name, callsign, colors, brightness, dim
        WiFi         - SSID/password, channel hop interval,
                       lock time, deauth toggle, RSSI thresholds
        GPS          - source (Grove/CapLoRa/Custom), baud rate,
                       timezone offset, power save
        BLE          - burst interval, advertisement duration
        Display      - brightness, dim timeout, dim level
        Boot Mode    - auto-start mode (IDLE/OINK/DNOHAM/WARHOG)
        G0 Button    - configurable action (screen toggle, mode entry)

    TOOLS:
        DIAGNOSTICS  - heap status, WiFi reset, garbage collection,
                       NTP sync status
        SD FORMAT    - nuclear option (confirm required)
        CRASH VIEWER - browse/delete core dumps
        UNLOCKABLES  - secret code entry portal


----[ 7.2 - LOOT > CAPTURES

    shows .22000 and .pcap files.
    per-capture: BSSID, type (PMKID/HS), WPA-SEC status, timestamp.

    [S] sync with WPA-SEC
    [D] nuke all captures (confirm required)
    [ENTER] detail view


----[ 7.3 - LOOT > WIGLE

    shows .wigle.csv files.
    per-file: filename, size, upload status.

    [S] sync with WiGLE
    [D] nuke selected track
    [ENTER] detail view

------------------------------------------------------------------------

--[ 8 - CONTROLS


----[ 8.1 - FROM IDLE

    [O] OINK MODE         active hunt
    [D] DO NO HAM         passive recon
    [W] WARHOG            wardriving
    [H] SPECTRUM          RF analysis
    [B] PIGGY BLUES       BLE chaos
    [A] BACON MODE        beacon spam
    [F] FILE TRANSFER     web file manager
    [S] SWINE STATS       numbers go up
    [T] SETTINGS          configuration

    [1] PIG DEMANDS       session challenges overlay
    [2] PIGSYNC           device discovery and sync


----[ 8.2 - NAVIGATION

    [;] or [UP]     scroll up / previous item
    [.] or [DOWN]   scroll down / next item
    [ENTER]         select / confirm / commit
    [`] or [BKSP]   back / cancel (context-aware, goes up one level)


----[ 8.3 - GLOBAL

    [P]  screenshot (saves to /m5porkchop/screenshots/)
    [G0] configurable magic button (set in settings)


----[ 8.4 - MODE-SPECIFIC

    OINK:
        [D] flip to DO NO HAM
        [B] add network to boar bros

    DO NO HAM:
        [D] flip to OINK

    SPECTRUM:
        [,] [/]     pan frequency
        [;] [.]     cycle network
        [F]         cycle filter
        [ENTER]     enter client monitor
        [SPACE]     toggle dial lock (ADV only)

    SPECTRUM CLIENT MONITOR:
        [;] [.]     navigate clients
        [ENTER]     deauth selected client
        [W]         reveal mode (broadcast deauth)
        [B]         add network to boar bros and exit
        [`]         exit to spectrum view

    BACON:
        [1] [2] [3] TX tier selection

------------------------------------------------------------------------

--[ 9 - SD CARD LAYOUT

    FAT32. 32GB or less preferred.
    the pig is organized.


----[ 9.1 - DIRECTORY STRUCTURE

    /m5porkchop/
        /config/
            porkchop.conf           JSON (WiFi, GPS, behavior)
            personality.json        name, colors, customization
        /handshakes/
            *.22000                 hashcat format
            *.pcap                  Wireshark format
            *.txt                   metadata companions
        /wardriving/
            *.wigle.csv             WiGLE v1.6 format
        /screenshots/
            *.png                   press [P] to capture
        /logs/
            porkchop.log            session logs (debug builds)
        /crash/
            coredump_*.elf          ESP-IDF core dumps
            coredump_*.txt          human-readable summaries
        /diagnostics/
            *.txt                   heap dumps
        /wpa-sec/
            wpasec_key.txt          API key (auto-deletes)
            results/                cracked passwords
            uploaded.txt            upload tracking
            sent.txt                submission records
        /wigle/
            wigle_key.txt           API key (auto-deletes)
            stats.json              cached user stats
            uploaded.txt            upload tracking
        /xp/
            xp_backup.bin           signed, device-bound XP
            awarded.txt             achievement tracking
        /misc/
            boar_bros.txt           whitelist (BSSID + SSID)
        /meta/
            migration_v1.marker     migration tracking


----[ 9.2 - LEGACY LAYOUT

    files in root (/, /handshakes/, /wardriving/) still supported.
    auto-migrated to /m5porkchop/ on boot.
    legacy backed up to /m5porkchop_backup/.
    the pig doesn't abandon its roots.

------------------------------------------------------------------------

--[ 10 - BUILDING

    PlatformIO. espressif32@6.12.0. Arduino framework.


----[ 10.1 - QUICK BUILD

    $ pip install platformio
    $ pio run -e m5cardputer
    $ pio run -t upload -e m5cardputer

    debug build (SD logging, verbose output):
    $ pio run -e m5cardputer-debug

    run unit tests (native platform, no hardware):
    $ pio test -e native

    tests with coverage:
    $ pio test -e native_coverage

    create release binaries:
    $ python scripts/build_release.py


----[ 10.2 - FROM RELEASE (recommended)

    github.com/0ct0sec/M5PORKCHOP/releases

    1. download firmware.bin
    2. SD card -> M5 Launcher -> install
    3. oink

    XP preserves across firmware updates via M5 Launcher.
    M5Burner nukes NVS. use Launcher. the pig insists.


----[ 10.3 - IF IT BREAKS

    "fatal error: M5Unified.h: No such file or directory"
        -> `git submodule update --init --recursive`

    "Connecting........_____....."
        -> hold BOOT button while connecting
        -> or the USB cable is trash. it's always the cable.

    "error: 'class WiFiClass' has no member named 'mode'"
        -> wrong board. we're ESP32-S3 (m5stack-stamps3).

    "Sketch too big"
        -> remove your debug printfs. ship clean.

    "undefined reference to '__sync_synchronize'"
        -> `pio run -t clean && pio run -e m5cardputer`

    still broken?
        github.com/0ct0sec/M5PORKCHOP/issues
        the confessional is open. bring logs.


----[ 10.4 - DEPENDENCIES

    m5stack/M5Unified         ^0.2.11
    m5stack/M5Cardputer       ^1.1.1
    bblanchon/ArduinoJson     ^7.4.2
    mikalhart/TinyGPSPlus     ^1.0.3
    h2zero/NimBLE-Arduino     ^2.3.7

    partition: 3MB app0 + 3MB app1 (OTA) + 1.5MB SPIFFS + 64KB coredump
    key build flags: -Os, -Wl,-zmuldefs (raw frame sanity check override)
    NimBLE: internal RAM only, log level ERROR

------------------------------------------------------------------------

--[ 11 - LEGAL

    this section is serious. the pig is serious here.


----[ 11.1 - EDUCATIONAL USE ONLY

    PORKCHOP exists for:
        - learning WiFi security concepts
        - authorized penetration testing
        - security research on YOUR infrastructure
        - understanding 802.11 at the frame level

    PORKCHOP does NOT exist for:
        - attacking networks you don't own or lack written permission for
        - tracking people without consent
        - being a nuisance in public spaces
        - impressing people (it won't. they'll just be confused.)


----[ 11.2 - CAPABILITIES VS RIGHTS

    DEAUTH: a capability, not a right.
    attacking networks you don't own is a CRIME.

    CLIENT TRACKING: a capability, not a right.
    tracking people without consent is STALKING.

    BEACON SPAM: a capability, not a right.
    beacon spam in public spaces is antisocial and possibly illegal.

    BLE SPAM: a capability, not a right.
    notification spam is annoying and potentially illegal.


----[ 11.3 - JURISDICTION

    USA:       CFAA. federal crime. don't.
    UK:        Computer Misuse Act 1990. still active.
    Germany:   StGB 202a-c. the Germans are thorough.
    Australia: Criminal Code Act 1995. even the emus judge.
    Japan:     Unauthorized Computer Access Law. very strict.
    Canada:    Criminal Code 342.1. polite but firm.

    the pig is not a lawyer.
    consult an actual lawyer.


----[ 11.4 - THE BOTTOM LINE

    tools don't make choices. YOU do.
    make good choices.

    the pig doesn't judge. the law does.
    the pig watches. the pig remembers.
    the pig will look disappointed in you.

    don't disappoint the pig.

    don't be stupid. don't be evil.
    don't make us regret publishing this.

    the confessional is open:
    github.com/0ct0sec/M5PORKCHOP/issues

------------------------------------------------------------------------

--[ 12 - TROUBLESHOOTING (the confessional)

    before you open an issue:

    [ ] did you read this README?
    [ ] actually read it, not skim it?
    [ ] did you check GitHub issues?
    [ ] did you try turning it off and on again?
    [ ] is the SD card FAT32, 32GB or less?
    [ ] is the SD card physically inserted?
        (zero judgment. okay, some judgment.)


----[ 12.1 - COMMON ISSUES

    "pig won't boot"
        -> reflash firmware via M5 Launcher
        -> try a different USB cable (it's always the cable)
        -> check power source (USB hub? try direct port)
        -> 3 rapid reboots = boot guard forces IDLE mode

    "XP gone after reflash"
        -> M5Burner nukes NVS. that's where XP lives.
        -> if you had SD card: XP restores from backup on boot
        -> if you didn't: BACON N00B. our condolences.
        -> use M5 Launcher next time. it preserves NVS.

    "WiFi won't connect in File Transfer"
        -> check SSID/password in settings
        -> 2.4GHz only. 5GHz not supported. the pig is old school.
        -> mDNS: porkchop.local (give it 5-10 seconds)

    "GPS won't lock"
        -> GPS needs sky view. go outside.
        -> check GPS source setting (Grove vs CapLoRa)
        -> default baud: 115200 (configurable in settings)

    "uploads fail / Not Enough Heap"
        -> TLS needs ~35KB contiguous
        -> exit OINK/DNH first (stop promiscuous mode)
        -> check DIAGNOSTICS for heap status
        -> reboot clears fragmentation

    "pig looks sad"
        -> feed it captures
        -> take it for a wardrive
        -> you're both going through something


----[ 12.2 - NUCLEAR OPTIONS

    1. reflash firmware (M5 Launcher, SD card)
    2. format SD card (FAT32, 32GB or less)
    3. TOOLS > SD FORMAT (from device, confirm required)
    4. open GitHub issue with:
       - firmware version (shown on boot splash)
       - hardware (original Cardputer or ADV)
       - steps to reproduce
       - screenshots ([P] key)
       - serial output if possible (115200 baud)

------------------------------------------------------------------------

--[ 13 - GREETZ


----[ 13.1 - INSPIRATIONS

    evilsocket + pwnagotchi
        the original. we're standing on shoulders.

    Phrack
        the formatting. the energy.
        if you know, you know.

    2600
        the spirit. we remember where we came from.

    Dark Souls / Elden Ring
        YOU DIED but we tried again.
        the gameplay loop of firmware development.


----[ 13.2 - THE ENABLERS

    the ESP32 underground
        the nerds who document the undocumented.
        the real heroes of promiscuous mode.

    the pigfarmers
        users who report bugs. your crash logs guide us.
        your patience sustains us. your expectations terrify us.


----[ 13.3 - THE RESIDENTS

    the horse
        structural consultant. barn inspector.
        k-hole enthusiast. the horse is the barn.

    the pig
        emotional support pwnagotchi.
        WiFi companion. judgment machine.
        better sleep hygiene than you.


----[ 13.4 - YOU

    you, for reading past the legal section.
    actually reading documentation is rare.
    the pig appreciates you. the horse might.
    but the horse is the barn and barns lack
    mechanisms for appreciation.

    coffee becomes code.
    code becomes bugs.
    bugs become trauma.
    trauma becomes coffee.
    https://buymeacoffee.com/0ct0

    the circle is complete.

    praise the sun.

    oink.

==[EOF]==
