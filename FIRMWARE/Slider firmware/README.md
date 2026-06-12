# MotionCtrl — Firmware ESP32-S3

Contrôleur de 2 moteurs pas-à-pas (A4988) avec interface web WiFi et manette déportée.

---

## Architecture

```
motionctrl/              ← Firmware unité principale
├── main/
│   ├── main.c           ← Point d'entrée, init, boucle watchdog
│   └── config.h         ← Tous les pins & constantes
└── components/
    ├── stepper/         ← Driver A4988 (esp_timer ISR, rampe accél)
    ├── motion_planner/  ← Points, séquences, effets, NVS
    ├── wifi_ap/         ← Point d'accès WiFi
    ├── webserver/       ← HTTP + WebSocket, UI embarquée
    └── controller_link/ ← UART vers manette (JSON framé)

motionctrl_controller/   ← Firmware manette
└── main/main.c          ← Joysticks, boutons, écran, UART
```

---

## Câblage unité principale

| Signal       | GPIO |
|-------------|------|
| M1 DIR       | 1    |
| M1 STEP      | 2    |
| M1 ENABLE    | 3    |
| M2 DIR       | 4    |
| M2 STEP      | 5    |
| M2 ENABLE    | 6    |
| ENDSTOP      | 13   |
| UART TX → manette | 17 |
| UART RX ← manette | 18 |

> A4988 : ENABLE actif à l'état BAS. Le firmware gère le HIGH/LOW automatiquement.
> ENDSTOP : connecte la broche à GND quand déclenché (pull-up interne activé).

---

## Câblage manette (ESP32-S3)

| Signal       | GPIO |
|-------------|------|
| Joystick 1 X | 1 (ADC1_CH0) |
| Joystick 1 BTN | 3 |
| Joystick 2 X | 4 (ADC1_CH3) |
| Joystick 2 BTN | 6 |
| BTN STOP     | 7  |
| BTN HOME     | 8  |
| BTN RUN      | 9  |
| BTN MENU     | 16 |
| UART TX → principale | 17 |
| UART RX ← principale | 18 |
| Écran BL PWM | 15 |
| Écran SPI MOSI | 11 |
| Écran SPI SCLK | 12 |
| Écran CS     | 10 |
| Écran DC     | 13 |
| Écran RST    | 14 |

---

## Compilation et flash

### Prérequis
- ESP-IDF v5.1+
- `idf.py` dans le PATH

### Unité principale
```bash
cd motionctrl
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### Manette
```bash
cd motionctrl_controller
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB1 flash monitor
```

---

## Utilisation

1. **Allumer l'unité principale** → crée le réseau WiFi `MotionCtrl` (mdp: `motionctrl123`)
2. **Connecter le téléphone** à ce réseau
3. **Ouvrir** http://192.168.4.1 dans le navigateur

### Interface web – onglets

| Onglet | Fonctionnalité |
|--------|---------------|
| **Jog** | Déplacement manuel moteur par moteur, homing, enable/disable |
| **Points** | Sauvegarder la position courante, lister, naviguer, supprimer |
| **Séquence** | Ajouter des points dans un ordre, choisir vitesse/pause/courbe d'accélération, lancer/pause/stop |
| **Effets** | Hyperlapse, Oscillation, Pendulum – configurer et lancer |

### Courbes d'accélération disponibles
- **Linéaire** : rampe constante
- **Ease In** : démarrage doux, freinage brusque
- **Ease Out** : démarrage brusque, freinage doux
- **Ease In/Out** : courbe S symétrique
- **Bézier** : smooth step (smoothstep cubique)

### Effets
| Effet | Description |
|-------|------------|
| **Hyperlapse** | Déplace en N positions équidistantes entre A et B, avec pause configurable entre chaque → parfait pour timelapse |
| **Oscillation** | Fait des allers-retours entre deux points, avec pause aux extrémités |
| **Pendulum** | Mouvement sinusoïdal entre deux points, vitesse variable naturellement |

### Manette physique
| Contrôle | Action |
|---------|--------|
| Joystick 1 | Jog Moteur 1 (proportionnel) |
| Joystick 2 | Jog Moteur 2 (proportionnel) |
| BTN STOP   | Arrêt immédiat des moteurs |
| BTN HOME   | Homing Moteur 1 |
| BTN RUN    | Lancer la séquence |
| BTN MENU   | Changer d'écran |
| JOY1 BTN  | Pause / Reprendre |

---

## Personnalisation

### Mécanique (config.h)
```c
#define DEFAULT_STEPS_PER_MM    80     // steps/mm selon votre mécanique
#define DEFAULT_MAX_SPEED_MM_S  100.0  // vitesse max mm/s
#define DEFAULT_ACCEL_MM_S2     200.0  // accélération mm/s²
```

### Changer le mot de passe WiFi
```c
#define WIFI_AP_PASS  "votre_mot_de_passe"
```

### Ajouter un driver d'écran (manette)
Remplacer la fonction `display_render()` dans `motionctrl_controller/main/main.c` par votre driver LCD (LVGL, TFT_eSPI, esp_lcd, etc.).

---

## Protocole UART (unité principale ↔ manette)

Frame : `[STX=0x02][LEN_HI][LEN_LO][JSON][ETX=0x03]`

### Commandes → unité principale
```json
{"cmd":"J","motor":0,"steps":100,"speed":10.0}   // Jog
{"cmd":"S"}                                        // Stop
{"cmd":"H","motor":0}                              // Home
{"cmd":"G","idx":2,"speed":15.0}                   // Goto point
{"cmd":"R"}                                        // Run sequence
{"cmd":"P"}                                        // Pause
{"cmd":"r"}                                        // Resume
```

### Status ← unité principale (push 200ms)
```json
{"state":"idle","m1_pos":1234,"m1_run":false,"m2_pos":567,"m2_run":false,"endstop":false}
```

---

## Notes importantes

- **Endstop** : le firmware déclenche un `stepper_stop_all()` sur interruption GPIO (front descendant). Si vous utilisez l'endstop pour le homing uniquement, vous pouvez retirer l'ISR dans `main.c`.
- **Microstepping** : le firmware compte en steps bruts. Ajustez `DEFAULT_STEPS_PER_MM` selon votre microstepping (1/16, 1/8, etc.).
- **La persistance NVS** sauvegarde automatiquement les points et séquences — ils survivent aux redémarrages.
- **Sécurité** : pas d'authentification sur l'interface web (réseau local isolé).
