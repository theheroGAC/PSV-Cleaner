#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/rtc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "psv_cleaner_core.h"
#include "psv_cleaner_tools.h"

SceRtcTick rtcTick;
int g_deletedFilesCount = 0;
int g_emergencyStop = 0;
int g_operationInProgress = 0;

ProgressCallback g_progressCallback = NULL;

static int g_lastProgressPercent = 0;

static void reportProgress(int pct) {
    g_lastProgressPercent = pct;
    if (g_progressCallback) {
        g_progressCallback(pct);
    }
}

unsigned long long g_cachedSpaceSize = 0;
int g_spaceCalculationNeeded = 1;
int g_lastCalculationFrame = 0;
#define CALCULATION_INTERVAL_FRAMES 60

int g_scanProgress = 0;
int g_totalScanItems = 0;
int g_currentScanItem = 0;

int excludePictureFolder = 1;
int excludeVpkFiles = 0;
int excludeVitaDBCache = 0;
int excludeVideoFolder = 1;

int cleanVitaShell = 1;
int cleanRetroArch = 1;
int cleanAdrenaline = 1;
int cleanBrowser = 1;
int cleanSystem = 1;
int cleanOrphanedData = 0;
int cleanAllAppsTempFiles = 0;
int cleanPkgi = 1;
int cleanAutoplugin = 1;
int cleanCrashDumps = 1;

int cleanEasyVpK = 1;
int cleanDaemon = 1;
int cleanVitaGrafix = 1;
int cleanOnemenu = 1;
int cleanPCSX = 1;
int cleanMGBA = 1;
int cleanFlycast = 1;
int cleanShellbat = 1;
int cleanSwitchUser = 1;
int cleanITLS = 1;
int cleanVHBB = 1;
int cleanPSVitaDB = 1;
int cleanDownloadEnabler = 1;
int cleanMoonlight = 1;
int cleanRetroFlow = 1;
int cleanHenkaku = 1;
int cleanPSVshell = 1;
int cleanCheatTools = 1;

int cleanThemeCache = 1;
int cleanNotificationCache = 1;
int cleanActivityLog = 1;
int cleanPhotoMusicCache = 1;
int cleanSceShellCache = 1;
int cleanFontCache = 1;
int cleanRegistryTemp = 1;
int cleanNetworkCache = 1;
int cleanLicenseCache = 1;
int cleanOrphanedLicenseFiles = 0;
int cleanOrphanedDLC = 0;
int cleanOrphanedAddcont = 0;
int cleanEmptyLiveareaBubbles = 0;
int cleanRePatchOrphans = 0;

#define CACHE_FILE_PATH "ux0:data/PSV_Cleaner/scan_cache.bin"
#define CACHE_VERSION 2
#define CACHE_EXPIRY_HOURS 24
#define MAX_CACHE_ENTRIES 320

typedef struct {
    char path[256];
    SceDateTime lastModified;
    unsigned long long totalSize;
    int fileCount;
    int isValid;
} CacheEntry;

typedef struct {
    int version;
    SceDateTime created;
    CacheEntry entries[MAX_CACHE_ENTRIES];
} ScanCache;

const char *TEMP_PATHS[] = {
    "ux0:temp/",
    "ux0:data/temp/",
    "ux0:cache/",
    "ux0:log/",
    "ux0:ptmp/",
    "ur0:temp/",
    "ur0:temp/sqlite/",
    "uma0:temp/",
    "uma0:ptmp/",
    "imc0:temp/",
    "imc0:cache/",
    "imc0:log/",
    "imc0:ptmp/",
    "imc0:data/temp/",
    "imc0:data/cache/",

    "ux0:VitaShell/cache/",
    "ux0:VitaShell/temp/",
    "ux0:VitaShell/log/",
    "ux0:VitaShell/recent/",
    "ux0:VitaShell/backup/temp/",
    "ux0:VitaShell/trash/",

    "ux0:pkgi/tmp/",
    "ux0:pkgi/cache/",
    "ux0:pkgi/log.txt",
    "ux0:pkgi/downloads/temp/",
    "ux0:pkgi/backup/temp/",
    "ux0:pkgi/*.vpk",

    "ux0:pkgj/tmp/",
    "ux0:pkgj/cache/",
    "ux0:pkgj/log.txt",
    "ux0:pkgj/downloads/temp/",
    "ux0:pkgj/*.pkg",
    "ux0:pkgj/*.tmp",
    "uma0:pkgj/tmp/",
    "uma0:pkgj/cache/",

    "ux0:*.vpk",
    "uma0:*.vpk",
    "imc0:*.vpk",

    "ux0:data/retroarch/cache/",
    "ux0:data/retroarch/logs/",
    "ux0:data/retroarch/temp/",
    "ux0:data/retroarch/thumbnails/cache/",
    "ux0:data/retroarch/shaders/cache/",
    "ux0:data/retroarch/database/rdb/temp/",
    "ux0:data/EMU4VITA/cache/",
    "ux0:data/EMU4VITA/logs/",
    "ux0:data/EMU4VITA/temp/",
    "ux0:data/EMU4VITAPLUS/cache/",
    "ux0:data/EMU4VITAPLUS/logs/",
    "ux0:data/EMU4VITAPLUS/temp/",

    "ux0:pspemu/temp/",
    "ux0:pspemu/cache/",

    "ux0:data/Adrenaline/cache/",
    "ux0:data/Adrenaline/logs/",
    "ux0:data/Adrenaline/temp/",
    "ux0:data/Adrenaline/crash/",
    "ux0:data/Adrenaline/dumps/",

    "ux0:data/moonlight/cache/",
    "ux0:data/moonlight/logs/",

    "ux0:data/AutoPlugin/cache/",
    "ux0:data/AutoPlugin/logs/",
    "ux0:data/AUTOPLUGIN2/cache/",
    "ux0:data/AUTOPLUGIN2/logs/",

    "ux0:data/RetroFlow/CACHE/",

    "ux0:data/henkaku/cache/",
    "ux0:data/henkaku/logs/",

    "ux0:data/PSVshell/logs/",
    "ux0:data/PSVshell/cache/",

    "ux0:data/savemgr/log/",
    "ux0:data/vitacheat/logs/",
    "ux0:data/rinCheat/logs/",

    "ux0:data/VitaDB/temp.tmp",
    "ux0:data/VitaDB/*.log",
    "ux0:data/VitaDB/*.tmp",
    "ux0:data/VitaDB/cache/",
    "ux0:vdb_data/",
    "ux0:data/vdb_vpk/",

    "ux0:data/browser/cache/",
    "ux0:data/browser/temp/",
    "ux0:data/browser/logs/",
    "ux0:data/webkit/cache/",
    "ux0:data/webkit/localstorage/temp/",

    "ux0:data/net/temp/",

    "ux0:download/",
    "ux0:download/temp/",
    "ux0:downloads/",
    "ux0:downloads/temp/",
    "ux0:bgdl/t/",
    "ux0:bgdl/w/",
    "uma0:bgdl/w/",
    "imc0:bgdl/w/",

    "ux0:data/pkg/temp/",
    "ux0:package/temp/",
    "ux0:appmeta/temp/",
    "ur0:appmeta/temp/",
    "ux0:license/temp/",
    "ux0:patch_temp/",
    "ux0:update_temp/",
    "ux0:SceShell/temp/",
    "ur0:shell/temp/",
    "ux0:registry/temp/",

    "ux0:picture/.thumbnails/",
    "ux0:picture/SCREENSHOT/",
    "ux0:video/.thumbnails/",
    "ux0:music/.cache/",
    "ux0:photo/cache/",

    "ux0:shaderlog/",

    "ux0:data/logs/temp/",
    "ux0:data/cache/",

    "uma0:cache/",
    "uma0:log/",
    "uma0:data/temp/",
    "uma0:data/cache/",
    "uma0:VitaShell/cache/",
    "uma0:VitaShell/temp/",
    "uma0:VitaShell/log/",
    "uma0:pkgi/cache/",
    "uma0:pkgi/log/",
    "uma0:data/retroarch/cache/",
    "uma0:data/retroarch/logs/",
    "uma0:data/retroarch/temp/",
    "uma0:data/EMU4VITA/cache/",
    "uma0:data/EMU4VITA/logs/",
    "uma0:data/EMU4VITA/temp/",
    "uma0:data/EMU4VITAPLUS/cache/",
    "uma0:data/EMU4VITAPLUS/logs/",
    "uma0:data/EMU4VITAPLUS/temp/",
    "uma0:data/Adrenaline/cache/",
    "uma0:data/Adrenaline/logs/",
    "uma0:data/Adrenaline/temp/",
    "uma0:data/moonlight/cache/",
    "uma0:data/moonlight/logs/",
    "uma0:data/AutoPlugin/cache/",
    "uma0:data/AutoPlugin/logs/",
    "uma0:data/AUTOPLUGIN2/cache/",
    "uma0:data/AUTOPLUGIN2/logs/",
    "uma0:data/RetroFlow/CACHE/",
    "uma0:data/henkaku/cache/",
    "uma0:data/henkaku/logs/",
    "uma0:data/PSVshell/logs/",
    "uma0:data/PSVshell/cache/",
    "uma0:data/savemgr/log/",
    "uma0:data/vitacheat/logs/",
    "uma0:data/rinCheat/logs/",
    "uma0:data/browser/cache/",
    "uma0:data/browser/temp/",
    "uma0:data/browser/logs/",
    "uma0:data/webkit/cache/",
    "uma0:data/webkit/localstorage/temp/",
    "uma0:data/net/temp/",
    "uma0:download/",
    "uma0:download/temp/",
    "uma0:downloads/",
    "uma0:downloads/temp/",
    "uma0:bgdl/t/",
    "uma0:data/pkg/temp/",
    "uma0:package/temp/",
    "uma0:appmeta/temp/",
    "uma0:data/logs/temp/",

    "ux0:data/psp2core*",
    "ux0:data/*.psp2core",
    "ux0:data/psp2dmp*",
    "ux0:data/*.psp2dmp",
    "ux0:data/crash_dumps/",
    "ux0:data/dumps/",
    "ux0:data/psp2core",
    "ux0:data/psp2dmp",
    "ur0:data/psp2core*",
    "ur0:data/*.psp2dmp",
    "ur0:temp/*.psp2dmp",

    "ux0:data/mgba/logs/",
    "uma0:data/mgba/cache/",
    "uma0:data/mgba/logs/",

    "ux0:data/pcsx_rearmed/logs/",
    "uma0:data/pcsx_rearmed/cache/",
    "uma0:data/pcsx_rearmed/logs/",

    "ux0:data/flycast/logs/",
    "ux0:data/flycast/temp/",
    "uma0:data/flycast/cache/",
    "uma0:data/flycast/logs/",
    "uma0:data/flycast/temp/",

    "ux0:data/vitaquake/logs/",
    "uma0:data/vitaquake/cache/",
    "uma0:data/vitaquake/logs/",

    "ux0:data/EasyVPK/cache/",
    "ux0:data/EasyVPK/temp/",

    "ux0:data/DAEMON/logs/",
    "uma0:data/DAEMON/cache/",
    "uma0:data/DAEMON/logs/",

    "uma0:data/iTLS/cache/",

    "uma0:data/DE/cache/",
    "ux0:data/ONEMenu/cache/",
    "uma0:data/ONEMenu/cache/",

    "uma0:data/shellbat/cache/",

    "uma0:data/switchuser/cache/",

    "uma0:data/PSVitaDB/cache/",

    "uma0:data/VitaGrafix/cache/",

    "ux0:VHBB/temp/",
    "uma0:VHBB/cache/",
    "uma0:VHBB/temp/",

    "ur0:shell/theme/cache/",
    "ux0:data/theme/temp/",

    "ur0:license/cache/",

    "ur0:config/np/",

    "ur0:temp/font/",
    "ux0:data/font/cache/",
    "ux0:data/activity/cache/",
    "ux0:notification/"
};

const size_t TEMP_PATHS_COUNT = sizeof(TEMP_PATHS)/sizeof(TEMP_PATHS[0]);

ThemeColors themes[MAX_THEMES] = {
    {
        .bg_primary = RGBA(15, 23, 42, 255),
        .bg_secondary = RGBA(2, 6, 23, 255),
        .bg_glow1 = RGBA(56, 189, 248, 35),
        .bg_glow2 = RGBA(99, 102, 241, 26),
        .card = RGBA(30, 41, 59, 235),
        .card_solid = RGBA(30, 41, 59, 255),
        .card_dark = RGBA(15, 23, 42, 240),
        .text_primary = RGBA(241, 245, 249, 255),
        .text_secondary = RGBA(148, 163, 184, 255),
        .text_faint = RGBA(100, 116, 139, 255),
        .accent = RGBA(56, 189, 248, 255),
        .accent_dim = RGBA(56, 189, 248, 120),
        .accent_soft = RGBA(56, 189, 248, 35),
        .success = RGBA(52, 211, 153, 255),
        .danger = RGBA(248, 113, 113, 255),
        .warning = RGBA(250, 204, 21, 255),
        .border = RGBA(51, 65, 85, 255),
        .row_alt = RGBA(51, 65, 85, 60),
        .selected = RGBA(56, 189, 248, 40)
    },
    {
        .bg_primary = RGBA(241, 245, 249, 255),
        .bg_secondary = RGBA(226, 232, 240, 255),
        .bg_glow1 = RGBA(14, 165, 233, 25),
        .bg_glow2 = RGBA(59, 130, 246, 22),
        .card = RGBA(255, 255, 255, 245),
        .card_solid = RGBA(255, 255, 255, 255),
        .card_dark = RGBA(226, 232, 240, 250),
        .text_primary = RGBA(15, 23, 42, 255),
        .text_secondary = RGBA(51, 65, 85, 255),
        .text_faint = RGBA(100, 116, 139, 255),
        .accent = RGBA(2, 132, 199, 255),
        .accent_dim = RGBA(2, 132, 199, 160),
        .accent_soft = RGBA(2, 132, 199, 30),
        .success = RGBA(5, 150, 105, 255),
        .danger = RGBA(220, 38, 38, 255),
        .warning = RGBA(202, 138, 4, 255),
        .border = RGBA(203, 213, 225, 255),
        .row_alt = RGBA(226, 232, 240, 120),
        .selected = RGBA(2, 132, 199, 35)
    },
    {
        .bg_primary = RGBA(0, 0, 0, 255),
        .bg_secondary = RGBA(0, 0, 0, 255),
        .bg_glow1 = RGBA(0, 229, 255, 18),
        .bg_glow2 = RGBA(0, 255, 170, 14),
        .card = RGBA(12, 12, 16, 245),
        .card_solid = RGBA(18, 18, 24, 255),
        .card_dark = RGBA(4, 4, 6, 255),
        .text_primary = RGBA(255, 255, 255, 255),
        .text_secondary = RGBA(180, 190, 205, 255),
        .text_faint = RGBA(110, 120, 135, 255),
        .accent = RGBA(0, 229, 255, 255),
        .accent_dim = RGBA(0, 229, 255, 140),
        .accent_soft = RGBA(0, 229, 255, 32),
        .success = RGBA(0, 230, 118, 255),
        .danger = RGBA(255, 82, 82, 255),
        .warning = RGBA(255, 215, 64, 255),
        .border = RGBA(38, 42, 52, 255),
        .row_alt = RGBA(24, 26, 34, 90),
        .selected = RGBA(0, 229, 255, 42)
    },
    {
        .bg_primary = RGBA(10, 32, 82, 255),
        .bg_secondary = RGBA(5, 15, 42, 255),
        .bg_glow1 = RGBA(66, 153, 225, 40),
        .bg_glow2 = RGBA(99, 179, 237, 30),
        .card = RGBA(16, 46, 108, 235),
        .card_solid = RGBA(20, 56, 128, 255),
        .card_dark = RGBA(8, 24, 62, 245),
        .text_primary = RGBA(245, 250, 255, 255),
        .text_secondary = RGBA(186, 212, 245, 255),
        .text_faint = RGBA(124, 158, 204, 255),
        .accent = RGBA(96, 210, 255, 255),
        .accent_dim = RGBA(96, 210, 255, 135),
        .accent_soft = RGBA(96, 210, 255, 40),
        .success = RGBA(72, 235, 168, 255),
        .danger = RGBA(255, 118, 118, 255),
        .warning = RGBA(255, 214, 70, 255),
        .border = RGBA(46, 94, 172, 255),
        .row_alt = RGBA(32, 72, 148, 70),
        .selected = RGBA(96, 210, 255, 48)
    }
};

int currentLanguage = LANGUAGE_EN;
int languageMode = LANGUAGE_AUTO;
AppTheme currentTheme = THEME_DARK;

const char* lang_titles_screen[MAX_LANGUAGES][15] = {
    {"PSV Cleaner", "Temporary Files Cleaner for PS Vita", "Select Cleaning Profile", "Advanced Cleaning Options", "Preview - Files to Delete", "Scanning files...", "CLEANING COMPLETED!", "Great job! Your PS Vita is cleaner!"},
    {"PSV Cleaner", "Pulizia file temporanei per PS Vita", "Seleziona Profilo", "Opzioni Avanzate", "Anteprima - File da Eliminare", "Scansione file...", "PULIZIA COMPLETATA!", "Ottimo lavoro! La tua PS Vita e piu pulita!"},
    {"PSV Cleaner", "Limpiador de archivos temporales para PS Vita", "Seleccionar Perfil", "Opciones Avanzadas", "Vista Previa", "Escaneando archivos...", "LIMPIEZA COMPLETADA!", "Buen trabajo! Tu PS Vita esta mas limpia!"},
    {"PSV Cleaner", "Nettoyeur de fichiers temporaires pour PS Vita", "Choisir un Profil", "Options Avancees", "Apercu", "Analyse des fichiers...", "NETTOYAGE TERMINE!", "Bravo! Votre PS Vita est plus propre!"},
    {"PSV Cleaner", "Temporaere Dateien-Reiniger fuer PS Vita", "Profil Waehlen", "Erweiterte Optionen", "Vorschau", "Scanne Dateien...", "REINIGUNG ABGESCHLOSSEN!", "Super! Deine PS Vita ist jetzt sauberer!"},
    {"PSV Cleaner", "Limpador de arquivos temporarios para PS Vita", "Selecionar Perfil", "Opcoes Avancadas", "Previa", "Analisando arquivos...", "LIMPEZA CONCLUIDA!", "Otimo trabalho! Seu PS Vita esta mais limpo!"},
    {"PSV Cleaner", "PS Vita 一時ファイルクリーナー", "モード選択", "詳細オプション", "プレビュー", "ファイルスキャン中...", "クリーンアップ完了!", "PS Vita がきれいになりました!"}
};

const char* lang_profile_options[MAX_LANGUAGES][6] = {
    {"Quick Clean", "Safe cache files only", "Complete Clean", "All temporary files", "Selective Clean", "Choose categories"},
    {"Pulizia Rapida", "Solo cache sicure", "Pulizia Completa", "Tutti i file temp", "Pulizia Selettiva", "Scegli categorie"},
    {"Limpieza Rapida", "Solo cache segura", "Limpieza Completa", "Todos temporales", "Limpieza Selectiva", "Elige categorias"},
    {"Nettoyage Rapide", "Caches surs uniquement", "Nettoyage Complet", "Tous temporaires", "Nettoyage Selectif", "Choisir categories"},
    {"Schnellreinigung", "Nur sichere Caches", "Komplettreinigung", "Alle Temp-Dateien", "Selektiv", "Kategorien waehlen"},
    {"Limpeza Rapida", "Apenas cache seguro", "Limpeza Completa", "Todos temporarios", "Limpeza Seletiva", "Escolher categorias"},
    {"クイッククリーン", "安全なキャッシュのみ", "完全クリーン", "全ての一時ファイル", "選択クリーン", "カテゴリを選択"}
};

const char* lang_ui_text[MAX_LANGUAGES][30] = {
    {"System Status:", "Ready for Cleanup", "Controls:", "Change Profile", "Preview & Clean", "Advanced Options", "Exit App", "Space to free:", "Space Freed:", "Files Deleted:", "D-Pad: Navigate | X: Select Profile | O: Exit", "Cleanup #", "No temporary files found!", "System Ready", "Version 1.18"},
    {"Stato Sistema:", "Pronto per la pulizia", "Controlli:", "Cambia Profilo", "Anteprima & Pulisci", "Opzioni Avanzate", "Esci", "Spazio da liberare:", "Spazio Liberato:", "File Eliminati:", "D-Pad: Naviga | X: Seleziona | O: Esci", "Pulizia #", "Nessun file temporaneo!", "Sistema Pronto", "Versione 1.18"},
    {"Estado Sistema:", "Listo para limpiar", "Controles:", "Cambiar Perfil", "Vista Previa", "Opciones Avanzadas", "Salir", "Espacio por liberar:", "Espacio Liberado:", "Archivos Borrados:", "D-Pad: Navegar | X: Seleccionar | O: Salir", "Limpieza #", "Sin archivos temporales!", "Sistema Listo", "Version 1.18"},
    {"Etat Systeme:", "Pret pour nettoyage", "Controles:", "Changer Profil", "Apercu", "Options Avancees", "Quitter", "Espace a liberer:", "Espace Libere:", "Fichiers Supprimes:", "D-Pad: Naviguer | X: Valider | O: Quitter", "Nettoyage #", "Aucun fichier trouve!", "Systeme Pret", "Version 1.18"},
    {"Systemstatus:", "Bereit zur Reinigung", "Steuerung:", "Profil Wechseln", "Vorschau", "Optionen", "Beenden", "Freizugeben:", "Freier Speicher:", "Geloescht:", "D-Pad: Waehlen | X: OK | O: Beenden", "Reinigung #", "Keine Temp-Dateien!", "System Bereit", "Version 1.18"},
    {"Estado Sistema:", "Pronto para limpeza", "Controles:", "Mudar Perfil", "Previa", "Opcoes Avancadas", "Sair", "Espaco a liberar:", "Espaco Liberado:", "Excluidos:", "D-Pad: Navegar | X: Selecionar | O: Sair", "Limpeza #", "Nenhum arquivo!", "Sistema Pronto", "Version 1.18"},
    {"システム状態:", "準備完了", "操作:", "モード変更", "プレビュー", "詳細設定", "終了", "解放容量:", "解放済み:", "削除数:", "十字キー: 移動 | X: 決定 | O: 終了", "回数 #", "ファイルなし", "準備完了", "Version 1.18"}
};

const char* L(const char* lang_array[][30], int lang_index, int text_index) {
    if (lang_index >= 0 && lang_index < MAX_LANGUAGES) {
        return lang_array[lang_index][text_index];
    }
    return lang_array[0][text_index];
}

static int mapSystemLang(int sysLang) {
    switch (sysLang) {
        case SCE_SYSTEM_PARAM_LANG_ITALIAN: return LANGUAGE_IT;
        case SCE_SYSTEM_PARAM_LANG_SPANISH: return LANGUAGE_ES;
        case SCE_SYSTEM_PARAM_LANG_FRENCH: return LANGUAGE_FR;
        case SCE_SYSTEM_PARAM_LANG_GERMAN: return LANGUAGE_DE;
        case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT:
        case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR: return LANGUAGE_PT;
        case SCE_SYSTEM_PARAM_LANG_JAPANESE: return LANGUAGE_JP;
        default: return LANGUAGE_EN;
    }
}

void detectSystemLanguage() {
    if (languageMode >= 0 && languageMode < MAX_LANGUAGES) {
        currentLanguage = languageMode;
        return;
    }
    int sysLang = SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
    if (sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &sysLang) >= 0) {
        currentLanguage = mapSystemLang(sysLang);
    } else {
        currentLanguage = LANGUAGE_EN;
    }
}

void setLanguageMode(int mode) {
    if (mode < LANGUAGE_AUTO || mode >= MAX_LANGUAGES) mode = LANGUAGE_AUTO;
    languageMode = mode;
    detectSystemLanguage();
}

const char* getLanguageName(int lang) {
    switch (lang) {
        case LANGUAGE_AUTO: return "Auto (System)";
        case LANGUAGE_EN: return "English";
        case LANGUAGE_IT: return "Italiano";
        case LANGUAGE_ES: return "Espanol";
        case LANGUAGE_FR: return "Francais";
        case LANGUAGE_DE: return "Deutsch";
        case LANGUAGE_PT: return "Portugues";
        case LANGUAGE_JP: return "Japanese";
        default: return "English";
    }
}

void setTheme(AppTheme theme) {
    if ((int)theme < 0 || (int)theme >= MAX_THEMES) theme = THEME_DARK;
    currentTheme = theme;
}

const char* getThemeName(AppTheme theme) {
    switch (theme) {
        case THEME_DARK: return "Dark Slate";
        case THEME_LIGHT: return "Light";
        case THEME_OLED: return "OLED Black";
        case THEME_VITA: return "Vita Blue";
        default: return "Dark Slate";
    }
}

void applyThemeColors(ThemeColors* colors) {
    if (!colors) return;
    int idx = (int)currentTheme;
    if (idx < 0 || idx >= MAX_THEMES) idx = 0;
    *colors = themes[idx];
}

int getDeletedFilesCount() {
    return g_deletedFilesCount;
}

int loadCleanupCounter() {
    int count = 0;
    SceUID fd = sceIoOpen("ux0:data/PSV_Cleaner/counter.txt", SCE_O_RDONLY, 0777);
    if (fd >= 0) {
        sceIoRead(fd, &count, sizeof(int));
        sceIoClose(fd);
    }
    return count;
}

void saveCleanupCounter(int count) {
    sceIoMkdir("ux0:data/PSV_Cleaner", 0777);
    SceUID fd = sceIoOpen("ux0:data/PSV_Cleaner/counter.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, &count, sizeof(int));
        sceIoClose(fd);
    }
}

void initEmergencyStop() {
    g_emergencyStop = 0;
    g_operationInProgress = 0;
}

void startOperation() {
    g_operationInProgress = 1;
    g_emergencyStop = 0;
    g_lastProgressPercent = 0;
    resetDeletedFilesCount();
    resetScanProgress();
    safetySetUserConfirmed(1);
}

void incrementDeletedFilesCount() {
    g_deletedFilesCount++;
}

void endOperation() {
    g_operationInProgress = 0;
    g_emergencyStop = 0;
    safetySetUserConfirmed(0);
}

void requestEmergencyStop() {
    g_emergencyStop = 1;
}

int isEmergencyStopRequested() {
    return g_emergencyStop;
}

int isOperationInProgress() {
    return g_operationInProgress;
}

void cleanupAfterEmergencyStop() {
    g_emergencyStop = 0;
    g_operationInProgress = 0;
    invalidateSpaceCache();
    clearScanCache();
    resetDeletedFilesCount();
    resetScanProgress();
    sceKernelDelayThread(200 * 1000);
}

void initScanProgress(int totalItems) {
    g_totalScanItems = totalItems;
    g_currentScanItem = 0;
    g_scanProgress = 0;
}

void updateScanProgress(int currentItem) {
    g_currentScanItem = currentItem;
    if (g_totalScanItems > 0) {
        g_scanProgress = (g_currentScanItem * 100) / g_totalScanItems;
    } else {
        g_scanProgress = 0;
    }
    reportProgress(g_scanProgress);
}

int getScanProgress() {
    return g_scanProgress;
}

int getLastProgressPercent() {
    return g_lastProgressPercent;
}

void resetScanProgress() {
    g_scanProgress = 0;
    g_currentScanItem = 0;
    g_totalScanItems = 0;
}

void resetDeletedFilesCount() {
    g_deletedFilesCount = 0;
}

void getCurrentTime(SceDateTime* dt) {
    SceRtcTick tick;
    sceRtcGetCurrentTick(&tick);
    sceRtcSetTick(dt, &tick);
}

int isCacheExpired(SceDateTime* cacheTime) {
    SceDateTime now;
    getCurrentTime(&now);

    SceRtcTick cacheTicks, nowTicks, expiryTicks;
    sceRtcGetTick(cacheTime, &cacheTicks);
    sceRtcGetTick(&now, &nowTicks);

    sceRtcTickAddHours(&expiryTicks, &cacheTicks, CACHE_EXPIRY_HOURS);

    return sceRtcCompareTick(&nowTicks, &expiryTicks) >= 0;
}

ScanCache* loadScanCache() {
    ScanCache* cache = (ScanCache*)malloc(sizeof(ScanCache));
    if (!cache) return NULL;

    SceUID fd = sceIoOpen(CACHE_FILE_PATH, SCE_O_RDONLY, 0777);
    if (fd < 0) {
        free(cache);
        return NULL;
    }

    int readSize = sceIoRead(fd, cache, sizeof(ScanCache));
    sceIoClose(fd);

    if (readSize != sizeof(ScanCache) || cache->version != CACHE_VERSION || isCacheExpired(&cache->created)) {
        free(cache);
        return NULL;
    }

    return cache;
}

void saveScanCache(ScanCache* cache) {
    if (!cache) return;

    sceIoMkdir("ux0:data/PSV_Cleaner", 0777);

    SceUID fd = sceIoOpen(CACHE_FILE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, cache, sizeof(ScanCache));
        sceIoClose(fd);
    }
}

static void updateMaxModTime(SceDateTime *best, const SceDateTime *candidate) {
    SceRtcTick bestTick, candTick;
    sceRtcGetTick(best, &bestTick);
    sceRtcGetTick(candidate, &candTick);
    if (sceRtcCompareTick(&candTick, &bestTick) > 0) {
        *best = *candidate;
    }
}

static int getDirectoryTreeModTime(const char* path, SceDateTime* modTime, int depth) {
    SceIoStat stat;
    if (sceIoGetstat(path, &stat) < 0) return 0;

    *modTime = stat.st_mtime;
    if (!SCE_S_ISDIR(stat.st_mode) || depth >= 2) return 1;

    SceUID dfd = sceIoDopen(path);
    if (dfd < 0) return 1;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));
    while (sceIoDread(dfd, &dir) > 0) {
        if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;
        updateMaxModTime(modTime, &dir.d_stat.st_mtime);
        if (SCE_S_ISDIR(dir.d_stat.st_mode) && depth + 1 < 2) {
            char sub[MAX_PATH_LENGTH];
            safe_snprintf(sub, sizeof(sub), "%s%s%s", path,
                          (path[strlen(path) - 1] == '/') ? "" : "/", dir.d_name);
            SceDateTime subTime;
            if (getDirectoryTreeModTime(sub, &subTime, depth + 1)) {
                updateMaxModTime(modTime, &subTime);
            }
        }
    }
    sceIoDclose(dfd);
    return 1;
}

int hasDirectoryChanged(const char* path, SceDateTime* cachedTime) {
    SceDateTime currentTime;
    if (!getDirectoryTreeModTime(path, &currentTime, 0)) {
        return 1;
    }

    SceRtcTick cachedTicks, currentTicks;
    sceRtcGetTick(cachedTime, &cachedTicks);
    sceRtcGetTick(&currentTime, &currentTicks);

    return sceRtcCompareTick(&currentTicks, &cachedTicks) > 0;
}

static int splitPatternDirFile(const char *pattern, char *dirOut, size_t dirSize, char *globOut, size_t globSize) {
    if (!pattern || strchr(pattern, '*') == NULL) return 0;

    const char *slash = strrchr(pattern, '/');
    const char *colon = strrchr(pattern, ':');
    const char *split = slash ? slash : colon;
    if (!split) return 0;

    size_t dirLen = (size_t)(split - pattern) + 1;
    if (dirLen >= dirSize) return 0;
    memcpy(dirOut, pattern, dirLen);
    dirOut[dirLen] = '\0';

    if (split == colon) {
        safe_strncat(dirOut, "/", dirSize);
    }

    safe_strncpy(globOut, split + 1, globSize);
    return globOut[0] != '\0';
}

static unsigned long long calculatePatternSize(const char *pattern) {
    char dirPath[MAX_PATH_LENGTH];
    char fileGlob[MAX_FILENAME_LENGTH];
    if (!splitPatternDirFile(pattern, dirPath, sizeof(dirPath), fileGlob, sizeof(fileGlob))) return 0;

    SceUID dfd = sceIoDopen(dirPath);
    if (dfd < 0) return 0;

    unsigned long long total = 0;
    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));

    while (sceIoDread(dfd, &dir) > 0) {
        if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
        if (!safetyMatchWildcard(fileGlob, dir.d_name)) continue;

        char fullPath[MAX_PATH_LENGTH];
        safe_snprintf(fullPath, sizeof(fullPath), "%s%s", dirPath, dir.d_name);
        if (whitelistCheck(fullPath, NULL)) continue;
        if (excludeVpkFiles && (safetyMatchWildcard("*.vpk", dir.d_name) || safetyMatchWildcard("*.pkg", dir.d_name))) continue;

        total += (unsigned long long)dir.d_stat.st_size;
    }

    sceIoDclose(dfd);
    return total;
}

static void deletePatternFiles(const char *pattern) {
    char dirPath[MAX_PATH_LENGTH];
    char fileGlob[MAX_FILENAME_LENGTH];
    if (!splitPatternDirFile(pattern, dirPath, sizeof(dirPath), fileGlob, sizeof(fileGlob))) return;

    SceUID dfd = sceIoDopen(dirPath);
    if (dfd < 0) return;

    char **matches = NULL;
    int count = 0, cap = 0;
    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));

    while (sceIoDread(dfd, &dir) > 0) {
        if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
        if (!safetyMatchWildcard(fileGlob, dir.d_name)) continue;

        if (count >= cap) {
            int nc = cap ? cap * 2 : 16;
            char **nm = (char **)realloc(matches, sizeof(char *) * nc);
            if (!nm) break;
            matches = nm;
            cap = nc;
        }
        matches[count] = strdup(dir.d_name);
        if (matches[count]) count++;
    }
    sceIoDclose(dfd);

    for (int i = 0; i < count; i++) {
        if (!isEmergencyStopRequested()) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "%s%s", dirPath, matches[i]);

            if (!whitelistCheck(fullPath, NULL) &&
                !(excludeVpkFiles && (safetyMatchWildcard("*.vpk", matches[i]) || safetyMatchWildcard("*.pkg", matches[i])))) {
                SafetyItem item;
                char reason[SAFETY_MAX_REASON];
                safetyItemInit(&item, fullPath, 0, SAFETY_ORPHAN_NONE);
                item.user_selected = 1;
                safetyRecompute(&item);
                if (safetyVerifyDelete(&item, reason, sizeof(reason))) {
                    if (sceIoRemove(fullPath) >= 0) {
                        g_deletedFilesCount++;
                    }
                }
            }
        }
        free(matches[i]);
    }
    if (matches) free(matches);
}

static void scanPatternForPreview(FileList *list, const char *pattern) {
    char dirPath[MAX_PATH_LENGTH];
    char fileGlob[MAX_FILENAME_LENGTH];
    if (!splitPatternDirFile(pattern, dirPath, sizeof(dirPath), fileGlob, sizeof(fileGlob))) return;

    SceUID dfd = sceIoDopen(dirPath);
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
        if (!safetyMatchWildcard(fileGlob, dir.d_name)) continue;

        char fullPath[MAX_PATH_LENGTH];
        safe_snprintf(fullPath, sizeof(fullPath), "%s%s", dirPath, dir.d_name);
        if (whitelistCheck(fullPath, NULL)) continue;
        if (excludeVpkFiles && (safetyMatchWildcard("*.vpk", dir.d_name) || safetyMatchWildcard("*.pkg", dir.d_name))) continue;

        addFileToList(list, fullPath, (unsigned long long)dir.d_stat.st_size);
    }

    sceIoDclose(dfd);
}

CacheEntry createCacheEntry(const char* path) {
    CacheEntry entry;
    memset(&entry, 0, sizeof(CacheEntry));
    safe_strncpy(entry.path, path, sizeof(entry.path));

    if (strchr(path, '*') != NULL) {
        char dirPath[MAX_PATH_LENGTH], fileGlob[MAX_FILENAME_LENGTH];
        if (splitPatternDirFile(path, dirPath, sizeof(dirPath), fileGlob, sizeof(fileGlob)) &&
            getDirectoryTreeModTime(dirPath, &entry.lastModified, 0)) {
            entry.totalSize = calculatePatternSize(path);
            entry.isValid = 1;
        }
        return entry;
    }

    if (getDirectoryTreeModTime(path, &entry.lastModified, 0)) {
        entry.totalSize = calculateTempSizeRecursive(path);
        entry.isValid = 1;
    } else {
        entry.isValid = 0;
    }

    return entry;
}

void clearScanCache() {
    sceIoRemove(CACHE_FILE_PATH);
}

void forceDeleteDumpFiles() {
    const char* dumpPatterns[] = {
        "ux0:data/psp2core",
        "ux0:data/psp2dmp",
        "ux0:data/psp2core*",
        "ux0:data/psp2dmp*",
        "ux0:data/*.psp2core",
        "ux0:data/*.psp2dmp",
        "ur0:data/psp2core*",
        "ur0:data/*.psp2dmp",
        "ur0:temp/*.psp2dmp"
    };

    for (size_t i = 0; i < sizeof(dumpPatterns) / sizeof(dumpPatterns[0]); i++) {
        if (strchr(dumpPatterns[i], '*') != NULL) {
            deletePatternFiles(dumpPatterns[i]);
        } else if (!whitelistCheck(dumpPatterns[i], NULL)) {
            for (int attempt = 0; attempt < 2; attempt++) {
                if (sceIoRemove(dumpPatterns[i]) >= 0) {
                    g_deletedFilesCount++;
                    break;
                }
                sceKernelDelayThread(50 * 1000);
            }
        }
    }

    const char* dumpDirs[] = {
        "ux0:data/crash_dumps/",
        "ux0:data/dumps/",
        "ux0:data/Adrenaline/dumps/",
        "ux0:data/Adrenaline/crash/"
    };

    for (size_t i = 0; i < sizeof(dumpDirs) / sizeof(dumpDirs[0]); i++) {
        if (!whitelistCheck(dumpDirs[i], NULL)) {
            deleteRecursive(dumpDirs[i]);
        }
    }
}

void aggressiveDumpCleanup() {
    const char* searchPaths[] = {
        "ux0:data/",
        "ux0:temp/",
        "ux0:cache/",
        "ux0:log/",
        "ur0:data/",
        "ur0:temp/",
        "uma0:data/",
        "uma0:temp/",
        "imc0:data/",
        "imc0:temp/"
    };

    for (size_t path = 0; path < sizeof(searchPaths) / sizeof(searchPaths[0]); path++) {
        if (isEmergencyStopRequested()) break;
        SceUID dfd = sceIoDopen(searchPaths[path]);
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (isEmergencyStopRequested()) break;
                if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
                if (!safetyIsDumpFileName(dir.d_name)) continue;

                char fullPath[MAX_PATH_LENGTH];
                safe_snprintf(fullPath, sizeof(fullPath), "%s%s", searchPaths[path], dir.d_name);
                if (whitelistCheck(fullPath, NULL)) continue;

                for (int attempt = 0; attempt < 3; attempt++) {
                    if (sceIoRemove(fullPath) >= 0) {
                        g_deletedFilesCount++;
                        break;
                    }
                    sceKernelDelayThread(100 * 1000);
                }
            }
            sceIoDclose(dfd);
        }
    }
}

void cleanupVpkFiles() {
    const char *roots[] = {"ux0:/", "uma0:/", "imc0:/"};

    for (size_t r = 0; r < sizeof(roots) / sizeof(roots[0]); r++) {
        if (isEmergencyStopRequested()) break;
        SceUID dfd = sceIoDopen(roots[r]);
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

                char* filename = dir.d_name;
                int len = (int)strlen(filename);
                if (len > 4 && (strcasecmp(filename + len - 4, ".vpk") == 0 || strcasecmp(filename + len - 4, ".pkg") == 0)) {
                    char fullPath[512];
                    snprintf(fullPath, sizeof(fullPath), "%s%s", roots[r], filename);
                    if (whitelistCheck(fullPath, NULL)) continue;

                    if (sceIoRemove(fullPath) >= 0) {
                        g_deletedFilesCount++;
                    }
                }
            }
            sceIoDclose(dfd);
        }
    }
}

typedef struct {
    char **names;
    int count;
    int capacity;
} DirEntries;

static int collectDirEntries(const char *path, DirEntries *entries) {
    SceUID dfd = sceIoDopen(path);
    if (dfd < 0) return 0;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0)
            continue;

        if (entries->count >= entries->capacity) {
            int newCap = entries->capacity ? entries->capacity * 2 : 64;
            char **newNames = (char**)realloc(entries->names, sizeof(char*) * newCap);
            if (!newNames) break;
            entries->names = newNames;
            entries->capacity = newCap;
        }

        entries->names[entries->count] = strdup(dir.d_name);
        if (entries->names[entries->count]) {
            entries->count++;
        }
    }
    sceIoDclose(dfd);
    return 1;
}

static void deleteRecursiveInternal(const char *path, int orphanApproved, const char *titleId);

void deleteRecursive(const char *path) {
    deleteRecursiveInternal(path, 0, NULL);
}

static int canDeletePath(const char *path, int orphanApproved, const char *titleId, unsigned long long size) {
    SafetyItem item;
    char reason[SAFETY_MAX_REASON];

    safetyItemInitWithTitle(&item, path, size,
                            orphanApproved ? SAFETY_ORPHAN_CERTAIN : SAFETY_ORPHAN_NONE,
                            titleId);
    item.user_selected = 1;
    safetyRecompute(&item);

    return safetyVerifyDelete(&item, reason, sizeof(reason));
}

static void deleteRecursiveInternal(const char *path, int orphanApproved, const char *titleId) {
    if (isEmergencyStopRequested()) return;
    if (whitelistCheck(path, titleId)) return;

    DirEntries entries;
    memset(&entries, 0, sizeof(entries));

    if (!collectDirEntries(path, &entries)) {
        if (canDeletePath(path, orphanApproved, titleId, 0)) {
            if (sceIoRemove(path) >= 0) {
                g_deletedFilesCount++;
            }
        }
        return;
    }

    for (int i = 0; i < entries.count; i++) {
        if (isEmergencyStopRequested()) break;

        char *newPath = malloc(MAX_PATH_LENGTH);
        if (!newPath) {
            free(entries.names[i]);
            continue;
        }

        snprintf(newPath, MAX_PATH_LENGTH, "%s%s%s", path,
                 (path[strlen(path)-1] == '/') ? "" : "/",
                 entries.names[i]);

        SceIoStat st;
        if (sceIoGetstat(newPath, &st) >= 0) {
            if (SCE_S_ISDIR(st.st_mode)) {
                deleteRecursiveInternal(newPath, orphanApproved, titleId);
                sceIoRmdir(newPath);
            } else {
                if (canDeletePath(newPath, orphanApproved, titleId, (unsigned long long)st.st_size)) {
                    if (sceIoRemove(newPath) >= 0) {
                        g_deletedFilesCount++;
                    }
                }
            }
        }

        free(newPath);
        free(entries.names[i]);

        if ((i & 63) == 63) {
            sceKernelDelayThread(1000);
        }
    }

    free(entries.names);
    sceIoRmdir(path);
}

unsigned long long calculateTempSizeRecursive(const char *path) {
    if (whitelistCheck(path, NULL)) return 0;

    unsigned long long total = 0;
    SceUID dfd = sceIoDopen(path);
    if (dfd >= 0) {
        SceIoDirent *dir = malloc(sizeof(SceIoDirent));
        if (!dir) {
            sceIoDclose(dfd);
            return 0;
        }

        while (sceIoDread(dfd, dir) > 0) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                continue;

            char *newPath = malloc(MAX_PATH_LENGTH);
            if (!newPath) continue;

            snprintf(newPath, MAX_PATH_LENGTH, "%s%s%s", path,
                     (path[strlen(path)-1] == '/') ? "" : "/",
                     dir->d_name);

            if (SCE_S_ISDIR(dir->d_stat.st_mode)) {
                total += calculateTempSizeRecursive(newPath);
            } else if (!whitelistCheck(newPath, NULL)) {
                total += dir->d_stat.st_size;
            }
            free(newPath);
        }
        sceIoDclose(dfd);
        free(dir);
    } else {
        SceIoStat stat;
        if (sceIoGetstat(path, &stat) >= 0) {
            if (SCE_S_ISREG(stat.st_mode)) {
                total += stat.st_size;
            }
        }
    }

    return total;
}

unsigned long long calculateTempSizeOptimized() {
    if (g_spaceCalculationNeeded) {
        g_cachedSpaceSize = calculateTempSize();
        g_spaceCalculationNeeded = 0;
    }
    return g_cachedSpaceSize;
}

void invalidateSpaceCache() {
    g_spaceCalculationNeeded = 1;
}

void updateSpaceCacheIfNeeded(int currentFrame) {
    if (g_spaceCalculationNeeded ||
        (currentFrame - g_lastCalculationFrame) > CALCULATION_INTERVAL_FRAMES) {
        g_cachedSpaceSize = calculateTempSize();
        g_spaceCalculationNeeded = 0;
        g_lastCalculationFrame = currentFrame;
    }
}

int shouldCleanPath(const char *path) {
    if (!path || whitelistCheck(path, NULL)) return 0;

    if (excludePictureFolder && (strncasecmp(path, "ux0:picture/", 12) == 0 || strncasecmp(path, "ux0:photo/", 10) == 0)) return 0;
    if (excludeVideoFolder && strncasecmp(path, "ux0:video/", 10) == 0) return 0;
    if (excludeVpkFiles && (strstr(path, ".vpk") != NULL || strstr(path, ".pkg") != NULL)) return 0;

    if (strncasecmp(path, "ux0:data/VitaDB/", 16) == 0 ||
        strncasecmp(path, "ux0:vdb_data/", 13) == 0 ||
        strncasecmp(path, "ux0:data/vdb_vpk/", 17) == 0 ||
        strncasecmp(path, "ux0:data/PSVitaDB/", 18) == 0 ||
        strncasecmp(path, "uma0:data/PSVitaDB/", 19) == 0) {
        if (excludeVitaDBCache) return 0;
        return cleanPSVitaDB;
    }

    if (strstr(path, "VitaShell/")) return cleanVitaShell;
    if (strstr(path, "pkgi/") || strstr(path, "pkgj/")) return cleanPkgi;
    if (strstr(path, "retroarch/") || strstr(path, "emu4vita/") || strstr(path, "emu4vitaplus/") || strstr(path, "EMU4VITA/") || strstr(path, "EMU4VITAPLUS/")) return cleanRetroArch;
    if (strstr(path, "Adrenaline/") || strstr(path, "pspemu/")) return cleanAdrenaline;
    if (strstr(path, "browser/") || strstr(path, "webkit/")) return cleanBrowser;

    if (strstr(path, "EasyVPK/")) return cleanEasyVpK;
    if (strstr(path, "DAEMON/")) return cleanDaemon;
    if (strstr(path, "VitaGrafix/")) return cleanVitaGrafix;
    if (strstr(path, "ONEMenu/")) return cleanOnemenu;
    if (strstr(path, "pcsx_rearmed/")) return cleanPCSX;
    if (strstr(path, "mgba/")) return cleanMGBA;
    if (strstr(path, "flycast/")) return cleanFlycast;
    if (strstr(path, "shellbat/")) return cleanShellbat;
    if (strstr(path, "switchuser/")) return cleanSwitchUser;
    if (strstr(path, "PSVitaDB/")) return cleanPSVitaDB;
    if (strstr(path, "iTLS/")) return cleanITLS;
    if (strstr(path, "DE/")) return cleanDownloadEnabler;
    if (strstr(path, "VHBB/")) return cleanVHBB;
    if (strstr(path, "moonlight/")) return cleanMoonlight;
    if (strstr(path, "RetroFlow/")) return cleanRetroFlow;
    if (strstr(path, "henkaku/") || strstr(path, "tai/")) return cleanHenkaku;
    if (strstr(path, "PSVshell/")) return cleanPSVshell;
    if (strstr(path, "vitacheat/") || strstr(path, "rinCheat/") || strstr(path, "savemgr/")) return cleanCheatTools;

    if (strstr(path, "theme/")) return cleanThemeCache;
    if (strstr(path, "font/")) return cleanFontCache;
    if (strstr(path, "notification/")) return cleanNotificationCache;
    if (strstr(path, "activity/")) return cleanActivityLog;
    if (strstr(path, "SceShell/") || strstr(path, "shell/temp/")) return cleanSceShellCache;
    if (strstr(path, "registry/temp/")) return cleanRegistryTemp;
    if (strstr(path, "picture/") || strstr(path, "photo/") || strstr(path, "music/") || strstr(path, "video/")) return cleanPhotoMusicCache;
    if (strstr(path, "net/")) return cleanNetworkCache;
    if (strstr(path, "license/cache/") || strstr(path, "license/temp/")) return cleanLicenseCache;

    if (strstr(path, "psp2core") || strstr(path, "psp2dmp") ||
        strstr(path, "crash_dumps/") || strstr(path, "dumps/")) {
        return cleanCrashDumps;
    }

    if (strstr(path, "AutoPlugin/") || strstr(path, "AUTOPLUGIN2/")) return cleanAutoplugin;

    if (strstr(path, "temp/") || strstr(path, "cache/") || strstr(path, "log/") ||
        strstr(path, "bgdl/") || strstr(path, "ptmp/") || strstr(path, "package/temp/") ||
        strstr(path, "appmeta/temp/") || strstr(path, "patch_temp/") ||
        strstr(path, "update_temp/") || strstr(path, "shaderlog/") ||
        strstr(path, "np/") || strstr(path, "download")) {
        return cleanSystem;
    }

    return 1;
}

unsigned long long calculateTempSize() {
    unsigned long long total = 0;
    ScanCache* cache = loadScanCache();
    ScanCache* newCache = NULL;

    if (!cache) {
        newCache = (ScanCache*)malloc(sizeof(ScanCache));
        if (newCache) {
            memset(newCache, 0, sizeof(ScanCache));
            newCache->version = CACHE_VERSION;
            getCurrentTime(&newCache->created);
        }
    }

    initScanProgress((int)TEMP_PATHS_COUNT);

    for (size_t i = 0; i < TEMP_PATHS_COUNT; i++) {
        if (isEmergencyStopRequested()) break;

        if (!shouldCleanPath(TEMP_PATHS[i])) {
            updateScanProgress((int)(i + 1));
            continue;
        }

        unsigned long long pathSize = 0;
        int isPattern = (strchr(TEMP_PATHS[i], '*') != NULL);

        if (!isPattern && cache && i < MAX_CACHE_ENTRIES && cache->entries[i].isValid &&
            !hasDirectoryChanged(TEMP_PATHS[i], &cache->entries[i].lastModified)) {
            pathSize = cache->entries[i].totalSize;
        } else {
            if (isPattern) {
                pathSize = calculatePatternSize(TEMP_PATHS[i]);
            } else {
                pathSize = calculateTempSizeRecursive(TEMP_PATHS[i]);
            }

            if (newCache && i < MAX_CACHE_ENTRIES) {
                newCache->entries[i] = createCacheEntry(TEMP_PATHS[i]);
            }
        }

        total += pathSize;
        updateScanProgress((int)(i + 1));
    }

    if (newCache) {
        saveScanCache(newCache);
        free(newCache);
    }

    if (cache) {
        free(cache);
    }

    total += calculateOrphanedDataSize();
    total += calculateAllAppsTempFilesSize();
    total += calculateOrphanedDLCDataSize();
    total += calculateOrphanedAddcontSize();
    total += calculateOrphanedLicenseFilesSize();
    total += calculateOrphanedLicenseDirsSize();
    total += calculateOrphanedPatchDirsSize();
    total += calculateEmptyLiveareaBubblesSize();

    return total;
}

unsigned long long cleanTemporaryFiles() {
    resetDeletedFilesCount();
    unsigned long long bytesFreed = 0;

    reportProgress(5);
    if (cleanCrashDumps) {
        forceDeleteDumpFiles();
    }
    reportProgress(7);
    if (cleanCrashDumps) {
        aggressiveDumpCleanup();
    }
    reportProgress(10);

    if (!excludeVpkFiles) {
        cleanupVpkFiles();
    }
    reportProgress(12);

    for (size_t i = 0; i < TEMP_PATHS_COUNT; i++) {
        if (isEmergencyStopRequested()) break;

        if (!shouldCleanPath(TEMP_PATHS[i])) {
            reportProgress((int)(15 + (i * 70) / TEMP_PATHS_COUNT));
            continue;
        }

        if (strchr(TEMP_PATHS[i], '*') != NULL) {
            unsigned long long before = calculatePatternSize(TEMP_PATHS[i]);
            deletePatternFiles(TEMP_PATHS[i]);
            unsigned long long after = calculatePatternSize(TEMP_PATHS[i]);
            if (before > after) bytesFreed += (before - after);
        } else {
            unsigned long long before = calculateTempSizeRecursive(TEMP_PATHS[i]);
            deleteRecursive(TEMP_PATHS[i]);
            unsigned long long after = calculateTempSizeRecursive(TEMP_PATHS[i]);
            if (before > after) bytesFreed += (before - after);
        }

        reportProgress((int)(15 + (i * 70) / TEMP_PATHS_COUNT));
    }

    reportProgress(87);

    if (cleanOrphanedData) {
        findOrphanedDataDirectories();
    }
    reportProgress(90);

    if (cleanAllAppsTempFiles) {
        bytesFreed += cleanAllAppsTempFilesData();
    }
    reportProgress(93);

    if (cleanOrphanedDLC) {
        findOrphanedDLCData();
    }
    reportProgress(95);
    if (cleanOrphanedAddcont) {
        findOrphanedAddcont();
    }
    reportProgress(96);
    if (cleanOrphanedLicenseFiles) {
        findOrphanedLicenseFiles();
    }
    reportProgress(97);
    if (cleanEmptyLiveareaBubbles) {
        removeEmptyLiveareaBubbles();
    }
    reportProgress(98);
    if (cleanOrphanedData) {
        findOrphanedLicenseDirectories();
        findOrphanedPatchDirectories();
    }
    reportProgress(99);

    sceIoRemove(CACHE_FILE_PATH);
    reportProgress(100);
    g_bgSpaceFreed = bytesFreed;
    return bytesFreed;
}

unsigned long long cleanSelectedPreviewFiles(FileList *list) {
    if (!list || list->count <= 0) return 0;
    resetDeletedFilesCount();
    unsigned long long freed = 0;

    int selTotal = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->files[i].selected) selTotal++;
    }
    initScanProgress(selTotal > 0 ? selTotal : 1);

    int done = 0;
    for (int i = 0; i < list->count; i++) {
        if (isEmergencyStopRequested()) break;
        if (!list->files[i].selected) continue;

        SafetyItem item;
        char reason[SAFETY_MAX_REASON];
        safetyItemInit(&item, list->files[i].path, list->files[i].size, SAFETY_ORPHAN_NONE);
        item.user_selected = 1;
        safetyRecompute(&item);

        if (safetyVerifyDelete(&item, reason, sizeof(reason)) && sceIoRemove(list->files[i].path) >= 0) {
            freed += list->files[i].size;
            g_deletedFilesCount++;
            list->files[i].selected = 0;
        }
        done++;
        updateScanProgress(done);
    }

    sceIoRemove(CACHE_FILE_PATH);
    invalidateSpaceCache();
    g_bgSpaceFreed = freed;
    return freed;
}

unsigned long long cleanFilteredPreviewFiles(FileList *list, const char *fileFilter, const char *searchQuery) {
    if (!list || list->count <= 0) return 0;
    resetDeletedFilesCount();
    unsigned long long freed = 0;

    int matchTotal = 0;
    for (int i = 0; i < list->count; i++) {
        const char *fn = strrchr(list->files[i].path, '/');
        fn = fn ? fn + 1 : list->files[i].path;
        if (matchesFileFilter(fn, fileFilter) && matchesFileSearch(list->files[i].path, searchQuery)) {
            matchTotal++;
        }
    }
    initScanProgress(matchTotal > 0 ? matchTotal : 1);

    int done = 0;
    for (int i = 0; i < list->count; i++) {
        if (isEmergencyStopRequested()) break;
        const char *fn = strrchr(list->files[i].path, '/');
        fn = fn ? fn + 1 : list->files[i].path;
        if (!matchesFileFilter(fn, fileFilter) || !matchesFileSearch(list->files[i].path, searchQuery)) continue;

        SafetyItem item;
        char reason[SAFETY_MAX_REASON];
        safetyItemInit(&item, list->files[i].path, list->files[i].size, SAFETY_ORPHAN_NONE);
        item.user_selected = 1;
        safetyRecompute(&item);

        if (safetyVerifyDelete(&item, reason, sizeof(reason)) && sceIoRemove(list->files[i].path) >= 0) {
            freed += list->files[i].size;
            g_deletedFilesCount++;
        }
        done++;
        updateScanProgress(done);
    }

    sceIoRemove(CACHE_FILE_PATH);
    invalidateSpaceCache();
    g_bgSpaceFreed = freed;
    return freed;
}

FileList* createFileList() {
    FileList *list = (FileList*)malloc(sizeof(FileList));
    if (!list) return NULL;

    list->capacity = 100;
    list->count = 0;
    list->totalSize = 0;
    list->files = (FileInfo*)malloc(sizeof(FileInfo) * list->capacity);

    if (!list->files) {
        free(list);
        return NULL;
    }

    return list;
}

void freeFileList(FileList *list) {
    if (list) {
        if (list->files) {
            free(list->files);
        }
        free(list);
    }
}

void addFileToList(FileList *list, const char *path, unsigned long long size) {
    if (!list || !path) return;
    if (whitelistCheck(path, NULL)) return;

    if (list->count >= list->capacity) {
        list->capacity *= 2;
        FileInfo *newFiles = (FileInfo*)realloc(list->files, sizeof(FileInfo) * list->capacity);
        if (!newFiles) return;
        list->files = newFiles;
    }

    safe_strncpy(list->files[list->count].path, path, sizeof(list->files[list->count].path));
    list->files[list->count].size = size;
    list->files[list->count].selected = 0;
    list->totalSize += size;
    list->count++;
}

void scanPathForPreview(FileList *list, const char *path) {
    if (strchr(path, '*') != NULL) {
        scanPatternForPreview(list, path);
        return;
    }

    SceUID dfd;
    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    dfd = sceIoDopen(path);
    if (dfd >= 0) {
        while (sceIoDread(dfd, &dir) > 0) {
            if (isEmergencyStopRequested()) break;
            if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0)
                continue;

            char newPath[1024];
            snprintf(newPath, sizeof(newPath), "%s%s%s", path,
                     (path[strlen(path)-1] == '/') ? "" : "/",
                     dir.d_name);

            if (whitelistCheck(newPath, NULL)) continue;

            if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
                scanPathForPreview(list, newPath);
            } else {
                addFileToList(list, newPath, dir.d_stat.st_size);
            }
        }
        sceIoDclose(dfd);
    } else {
        SceIoStat stat;
        if (sceIoGetstat(path, &stat) >= 0) {
            if (!SCE_S_ISDIR(stat.st_mode) && !whitelistCheck(path, NULL)) {
                addFileToList(list, path, stat.st_size);
            }
        }
    }
}

int compareFilesByName(const void *a, const void *b) {
    const FileInfo *fileA = (const FileInfo *)a;
    const FileInfo *fileB = (const FileInfo *)b;
    return strcmp(fileA->path, fileB->path);
}

int compareFilesBySize(const void *a, const void *b) {
    const FileInfo *fileA = (const FileInfo *)a;
    const FileInfo *fileB = (const FileInfo *)b;
    if (fileA->size > fileB->size) return -1;
    if (fileA->size < fileB->size) return 1;
    return 0;
}

void sortFileList(FileList *list, SortMode sortMode) {
    if (!list || list->count <= 1) return;

    switch (sortMode) {
        case SORT_BY_NAME:
            qsort(list->files, list->count, sizeof(FileInfo), compareFilesByName);
            break;
        case SORT_BY_SIZE:
            qsort(list->files, list->count, sizeof(FileInfo), compareFilesBySize);
            break;
    }
}

int matchesFileFilter(const char *filename, const char *filter) {
    if (!filter || strlen(filter) == 0) return 1;
    if (!filename) return 0;

    if (strcasecmp(filter, "dmp") == 0) {
        if (safetyIsDumpFileName(filename)) return 1;
    }

    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;

    return strcasecmp(ext + 1, filter) == 0;
}

int matchesFileSearch(const char *path, const char *searchQuery) {
    if (!searchQuery || searchQuery[0] == '\0') return 1;
    if (!path) return 0;

    size_t qLen = strlen(searchQuery);
    size_t pLen = strlen(path);
    if (qLen > pLen) return 0;

    for (size_t i = 0; i <= pLen - qLen; i++) {
        size_t j = 0;
        while (j < qLen && tolower((unsigned char)path[i + j]) == tolower((unsigned char)searchQuery[j])) {
            j++;
        }
        if (j == qLen) return 1;
    }
    return 0;
}

void filterAndSortFileList(FileList *list, SortMode sortMode, const char *fileFilter, unsigned long long *totalVisibleSize) {
    if (!list) return;

    sortFileList(list, sortMode);

    if (totalVisibleSize) {
        *totalVisibleSize = 0;
        for (int i = 0; i < list->count; i++) {
            char *filename = strrchr(list->files[i].path, '/');
            if (filename) filename++;
            else filename = list->files[i].path;

            if (matchesFileFilter(filename, fileFilter) && matchesFileSearch(list->files[i].path, g_bgSearchQuery)) {
                *totalVisibleSize += list->files[i].size;
            }
        }
    }
}

int deleteSingleFileFromList(FileList *list, int index) {
    if (!list || index < 0 || index >= list->count) {
        return 0;
    }

    SafetyItem item;
    char reason[SAFETY_MAX_REASON];
    safetyItemInit(&item, list->files[index].path, list->files[index].size, SAFETY_ORPHAN_NONE);
    item.user_selected = 1;
    safetyRecompute(&item);
    safetySetUserConfirmed(1);
    int allowed = safetyVerifyDelete(&item, reason, sizeof(reason));
    safetySetUserConfirmed(0);
    if (!allowed) return 0;

    if (sceIoRemove(list->files[index].path) >= 0) {
        unsigned long long removedSize = list->files[index].size;

        for (int i = index; i < list->count - 1; i++) {
            list->files[i] = list->files[i + 1];
        }

        list->count--;
        list->totalSize -= removedSize;

        g_deletedFilesCount++;
        invalidateSpaceCache();
        historyAppend("Single File Delete", 1, 0, 0, removedSize);

        return 1;
    }

    return 0;
}

static int appListContains(char **apps, int count, const char *id) {
    for (int i = 0; i < count; i++) {
        if (strcasecmp(apps[i], id) == 0) return 1;
    }
    return 0;
}

void getInstalledAppsList(char ***apps, int *count) {
    *count = 0;
    *apps = NULL;

    const char *appRoots[] = {"ux0:app/", "uma0:app/", "imc0:app/"};

    for (size_t r = 0; r < sizeof(appRoots) / sizeof(appRoots[0]); r++) {
        SceUID dfd = sceIoDopen(appRoots[r]);
        if (dfd < 0) continue;

        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));

        while (sceIoDread(dfd, &dir) > 0) {
            if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
            if (!safetyIsValidTitleId(dir.d_name)) continue;
            if (appListContains(*apps, *count, dir.d_name)) continue;

            (*count)++;
            *apps = (char **)realloc(*apps, *count * sizeof(char *));
            (*apps)[*count - 1] = strdup(dir.d_name);
        }

        sceIoDclose(dfd);
    }
}

int isAppInstalled(const char *title_id) {
    return isTitleIdInstalled(title_id);
}

int isTitleIdInstalled(const char *titleId) {
    char path[MAX_PATH_LENGTH];
    SceIoStat stat;

    if (!safetyIsValidTitleId(titleId)) return 0;

    const char *checkTemplates[] = {
        "ux0:app/%s",
        "ux0:appmeta/%s",
        "ur0:app/%s",
        "ur0:appmeta/%s",
        "uma0:app/%s",
        "uma0:appmeta/%s",
        "imc0:app/%s",
        "imc0:appmeta/%s",
        "vs0:app/%s"
    };

    for (size_t i = 0; i < sizeof(checkTemplates) / sizeof(checkTemplates[0]); i++) {
        safe_snprintf(path, sizeof(path), checkTemplates[i], titleId);
        if (sceIoGetstat(path, &stat) >= 0) return 1;
    }

    return 0;
}

int isOlderThanDays(const SceDateTime *mtime, int days) {
    SceDateTime now;
    getCurrentTime(&now);

    SceRtcTick nowTicks, modTicks;
    sceRtcGetTick(&now, &nowTicks);
    sceRtcGetTick(mtime, &modTicks);

    SceRtcTick thresholdTicks;
    sceRtcTickAddDays(&thresholdTicks, &modTicks, days);

    return sceRtcCompareTick(&nowTicks, &thresholdTicks) >= 0;
}

void findOrphanedDataDirectories() {
    const char *dataRoots[] = {"ux0:data/", "uma0:data/", "imc0:data/"};

    for (size_t root = 0; root < sizeof(dataRoots) / sizeof(dataRoots[0]); root++) {
        SceUID dfd = sceIoDopen(dataRoots[root]);
        if (dfd < 0) continue;

        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));

        while (sceIoDread(dfd, &dir) > 0) {
            if (isEmergencyStopRequested()) break;
            if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

            char dirName[MAX_FILENAME_LENGTH];
            safe_strncpy(dirName, dir.d_name, sizeof(dirName));

            if (isSystemDataDirName(dirName)) continue;
            if (whitelistCheck(NULL, dirName)) continue;

            if (safetyIsValidTitleId(dirName) && !isTitleIdInstalled(dirName)) {
                char fullPath[512];
                snprintf(fullPath, sizeof(fullPath), "%s%s", dataRoots[root], dirName);
                if (whitelistCheck(fullPath, dirName)) continue;

                SceIoStat stat;
                if (sceIoGetstat(fullPath, &stat) >= 0 && isOlderThanDays(&stat.st_mtime, 30)) {
                    deleteRecursiveInternal(fullPath, 1, dirName);
                }
            }
        }
        sceIoDclose(dfd);
    }

    if (cleanRePatchOrphans) {
        const char *reRoots[] = {"ux0:rePatch/", "ux0:reAddcont/"};
        for (size_t root = 0; root < sizeof(reRoots) / sizeof(reRoots[0]); root++) {
            SceUID dfd = sceIoDopen(reRoots[root]);
            if (dfd < 0) continue;
            SceIoDirent dir;
            memset(&dir, 0, sizeof(dir));
            while (sceIoDread(dfd, &dir) > 0) {
                if (isEmergencyStopRequested()) break;
                if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
                if (safetyIsValidTitleId(dir.d_name) && !isTitleIdInstalled(dir.d_name) && isOlderThanDays(&dir.d_stat.st_mtime, 30)) {
                    char fullPath[MAX_PATH_LENGTH];
                    safe_snprintf(fullPath, sizeof(fullPath), "%s%s", reRoots[root], dir.d_name);
                    if (!whitelistCheck(fullPath, dir.d_name)) {
                        deleteRecursiveInternal(fullPath, 1, dir.d_name);
                    }
                }
            }
            sceIoDclose(dfd);
        }
    }
}

unsigned long long calculateOrphanedDataSize() {
    unsigned long long total = 0;
    if (!cleanOrphanedData) return 0;

    const char *dataRoots[] = {"ux0:data/", "uma0:data/", "imc0:data/"};

    for (size_t root = 0; root < sizeof(dataRoots) / sizeof(dataRoots[0]); root++) {
        SceUID dfd = sceIoDopen(dataRoots[root]);
        if (dfd < 0) continue;

        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));

        while (sceIoDread(dfd, &dir) > 0) {
            if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

            char dirName[MAX_FILENAME_LENGTH];
            safe_strncpy(dirName, dir.d_name, sizeof(dirName));

            if (isSystemDataDirName(dirName)) continue;
            if (whitelistCheck(NULL, dirName)) continue;

            if (safetyIsValidTitleId(dirName) && !isTitleIdInstalled(dirName)) {
                char fullPath[MAX_PATH_LENGTH];
                safe_snprintf(fullPath, sizeof(fullPath), "%s%s", dataRoots[root], dirName);
                if (whitelistCheck(fullPath, dirName)) continue;

                SceIoStat stat;
                if (sceIoGetstat(fullPath, &stat) < 0) continue;
                if (!isOlderThanDays(&stat.st_mtime, 30)) continue;

                total += calculateTempSizeRecursive(fullPath);
            }
        }
        sceIoDclose(dfd);
    }

    if (cleanRePatchOrphans) {
        const char *reRoots[] = {"ux0:rePatch/", "ux0:reAddcont/"};
        for (size_t root = 0; root < sizeof(reRoots) / sizeof(reRoots[0]); root++) {
            SceUID dfd = sceIoDopen(reRoots[root]);
            if (dfd < 0) continue;
            SceIoDirent dir;
            memset(&dir, 0, sizeof(dir));
            while (sceIoDread(dfd, &dir) > 0) {
                if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
                if (safetyIsValidTitleId(dir.d_name) && !isTitleIdInstalled(dir.d_name) && isOlderThanDays(&dir.d_stat.st_mtime, 30)) {
                    char fullPath[MAX_PATH_LENGTH];
                    safe_snprintf(fullPath, sizeof(fullPath), "%s%s", reRoots[root], dir.d_name);
                    if (!whitelistCheck(fullPath, dir.d_name)) {
                        total += calculateTempSizeRecursive(fullPath);
                    }
                }
            }
            sceIoDclose(dfd);
        }
    }

    return total;
}

unsigned long long calculateOrphanedLicenseDirsSize() {
    unsigned long long total = 0;
    if (!cleanOrphanedData) return 0;

    SceUID dfd = sceIoDopen("ux0:license/");
    if (dfd < 0) return 0;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
        if (strlen(dir.d_name) != 16 || isTitleIdInstalled(dir.d_name + 7)) continue;
        if (!isOlderThanDays(&dir.d_stat.st_mtime, 30)) continue;

        char fullPath[MAX_PATH_LENGTH];
        safe_snprintf(fullPath, sizeof(fullPath), "ux0:license/%s", dir.d_name);
        if (!whitelistCheck(fullPath, dir.d_name + 7)) {
            total += calculateTempSizeRecursive(fullPath);
        }
    }
    sceIoDclose(dfd);

    return total;
}

unsigned long long calculateOrphanedPatchDirsSize() {
    unsigned long long total = 0;
    if (!cleanOrphanedData) return 0;

    SceUID dfd = sceIoDopen("ux0:patch/");
    if (dfd < 0) return 0;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
        if (!safetyIsValidTitleId(dir.d_name) || isTitleIdInstalled(dir.d_name)) continue;
        if (!isOlderThanDays(&dir.d_stat.st_mtime, 30)) continue;

        char fullPath[MAX_PATH_LENGTH];
        safe_snprintf(fullPath, sizeof(fullPath), "ux0:patch/%s", dir.d_name);
        if (!whitelistCheck(fullPath, dir.d_name)) {
            total += calculateTempSizeRecursive(fullPath);
        }
    }
    sceIoDclose(dfd);

    return total;
}

void findOrphanedLicenseDirectories() {
    SceUID dfd = sceIoDopen("ux0:license/");
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char dirName[MAX_FILENAME_LENGTH];
        safe_strncpy(dirName, dir.d_name, sizeof(dirName));

        if (strlen(dirName) == 16 && !isTitleIdInstalled(dirName + 7)) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:license/%s", dirName);
            if (whitelistCheck(fullPath, dirName + 7)) continue;

            SceIoStat stat;
            if (sceIoGetstat(fullPath, &stat) >= 0 && isOlderThanDays(&stat.st_mtime, 30)) {
                deleteRecursiveInternal(fullPath, 1, dirName + 7);
            }
        }
    }
    sceIoDclose(dfd);
}

void findOrphanedPatchDirectories() {
    SceUID dfd = sceIoDopen("ux0:patch/");
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char dirName[MAX_FILENAME_LENGTH];
        safe_strncpy(dirName, dir.d_name, sizeof(dirName));

        if (safetyIsValidTitleId(dirName) && !isTitleIdInstalled(dirName)) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:patch/%s", dirName);
            if (whitelistCheck(fullPath, dirName)) continue;

            SceIoStat stat;
            if (sceIoGetstat(fullPath, &stat) >= 0 && isOlderThanDays(&stat.st_mtime, 30)) {
                deleteRecursiveInternal(fullPath, 1, dirName);
            }
        }
    }
    sceIoDclose(dfd);
}

int isSystemDataDirName(const char *dirName) {
    return strcmp(dirName, "Adrenaline") == 0 ||
        strcmp(dirName, "AutoPlugin") == 0 ||
        strcmp(dirName, "AUTOPLUGIN2") == 0 ||
        strcmp(dirName, "browser") == 0 ||
        strcmp(dirName, "RetroFlow") == 0 ||
        strcmp(dirName, "VitaDB") == 0 ||
        strcmp(dirName, "henkaku") == 0 ||
        strcmp(dirName, "moonlight") == 0 ||
        strcmp(dirName, "PSP2SHELL") == 0 ||
        strcmp(dirName, "PSVshell") == 0 ||
        strcmp(dirName, "RetroArch") == 0 ||
        strcmp(dirName, "VitaShell") == 0 ||
        strcmp(dirName, "savemgr") == 0 ||
        strcmp(dirName, "vitacheat") == 0 ||
        strcmp(dirName, "rinCheat") == 0 ||
        strcmp(dirName, "webkit") == 0 ||
        strcmp(dirName, "net") == 0 ||
        strcmp(dirName, "pkg") == 0 ||
        strcmp(dirName, "PSV_Cleaner") == 0 ||
        strcmp(dirName, "logs") == 0 ||
        strcmp(dirName, "cache") == 0;
}

void scanFilesForPreview(FileList *list) {
    if (!list) return;

    for (size_t i = 0; i < TEMP_PATHS_COUNT; i++) {
        if (isEmergencyStopRequested()) break;

        if (!shouldCleanPath(TEMP_PATHS[i])) {
            continue;
        }

        scanPathForPreview(list, TEMP_PATHS[i]);
    }

    const char* dumpSearchPaths[] = {
        "ux0:data/",
        "ux0:temp/",
        "ux0:cache/",
        "ux0:log/",
        "ur0:data/",
        "ur0:temp/",
        "uma0:data/",
        "uma0:temp/",
        "imc0:data/",
        "imc0:temp/"
    };

    for (size_t path = 0; path < sizeof(dumpSearchPaths) / sizeof(dumpSearchPaths[0]) && cleanCrashDumps; path++) {
        if (isEmergencyStopRequested()) break;
        SceUID dfd = sceIoDopen(dumpSearchPaths[path]);
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
                if (!safetyIsDumpFileName(dir.d_name)) continue;

                char fullPath[512];
                snprintf(fullPath, sizeof(fullPath), "%s%s", dumpSearchPaths[path], dir.d_name);
                addFileToList(list, fullPath, dir.d_stat.st_size);
            }
            sceIoDclose(dfd);
        }
    }

    if (!excludeVpkFiles) {
        const char *vpkRoots[] = {"ux0:/", "uma0:/", "imc0:/"};
        for (size_t r = 0; r < sizeof(vpkRoots) / sizeof(vpkRoots[0]); r++) {
            SceUID vpkDfd = sceIoDopen(vpkRoots[r]);
            if (vpkDfd >= 0) {
                SceIoDirent vpkDir;
                memset(&vpkDir, 0, sizeof(SceIoDirent));

                while (sceIoDread(vpkDfd, &vpkDir) > 0) {
                    if (SCE_S_ISDIR(vpkDir.d_stat.st_mode)) continue;

                    char* filename = vpkDir.d_name;
                    int len = (int)strlen(filename);
                    if (len > 4 && (strcasecmp(filename + len - 4, ".vpk") == 0 || strcasecmp(filename + len - 4, ".pkg") == 0)) {
                        char fullPath[512];
                        snprintf(fullPath, sizeof(fullPath), "%s%s", vpkRoots[r], filename);
                        addFileToList(list, fullPath, vpkDir.d_stat.st_size);
                    }
                }
                sceIoDclose(vpkDfd);
            }
        }
    }

    if (cleanAllAppsTempFiles) {
        char **apps = NULL;
        int appCount = 0;
        getInstalledAppsList(&apps, &appCount);

        if (apps) {
            for (int i = 0; i < appCount; i++) {
                if (!isEmergencyStopRequested()) {
                    scanAppTempFilesForPreview(list, apps[i]);
                }
                free(apps[i]);
            }
            free(apps);
        }
    }

    if (cleanOrphanedData) {
        const char *dataRoots[] = {"ux0:data/", "uma0:data/", "imc0:data/"};
        for (size_t r = 0; r < sizeof(dataRoots) / sizeof(dataRoots[0]); r++) {
            SceUID dfd = sceIoDopen(dataRoots[r]);
            if (dfd < 0) continue;

            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (isEmergencyStopRequested()) break;
                if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
                if (isSystemDataDirName(dir.d_name)) continue;
                if (!safetyIsValidTitleId(dir.d_name) || isTitleIdInstalled(dir.d_name)) continue;

                char fullPath[MAX_PATH_LENGTH];
                safe_snprintf(fullPath, sizeof(fullPath), "%s%s", dataRoots[r], dir.d_name);
                if (whitelistCheck(fullPath, dir.d_name)) continue;

                SceIoStat stat;
                if (sceIoGetstat(fullPath, &stat) < 0) continue;
                if (!isOlderThanDays(&stat.st_mtime, 30)) continue;

                scanPathForPreview(list, fullPath);
            }
            sceIoDclose(dfd);
        }
    }

    if (cleanOrphanedDLC || cleanOrphanedAddcont) {
        SceUID dfd = sceIoDopen("ux0:addcont/");
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (isEmergencyStopRequested()) break;
                if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

                if (strlen(dir.d_name) == 9) {
                    if (!cleanOrphanedDLC || isTitleIdInstalled(dir.d_name)) continue;
                } else {
                    if (!cleanOrphanedAddcont) continue;
                }

                char fullPath[MAX_PATH_LENGTH];
                safe_snprintf(fullPath, sizeof(fullPath), "ux0:addcont/%s", dir.d_name);
                if (!whitelistCheck(fullPath, dir.d_name)) {
                    scanPathForPreview(list, fullPath);
                }
            }
            sceIoDclose(dfd);
        }
    }

    if (cleanOrphanedLicenseFiles) {
        SceUID dfd = sceIoDopen("ux0:license/");
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (isEmergencyStopRequested()) break;
                if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

                char *filename = dir.d_name;
                int len = (int)strlen(filename);
                if (len <= 4 || strcasecmp(filename + len - 4, ".rif") != 0) continue;

                int isOrphaned = 0;
                if (len == 20) {
                    char titleId[10] = {0};
                    memcpy(titleId, filename, 9);
                    titleId[9] = '\0';
                    if (!isTitleIdInstalled(titleId)) isOrphaned = 1;
                } else if (len == 16 && isOlderThanDays(&dir.d_stat.st_mtime, 60)) {
                    isOrphaned = 1;
                }

                if (isOrphaned) {
                    char fullPath[MAX_PATH_LENGTH];
                    safe_snprintf(fullPath, sizeof(fullPath), "ux0:license/%s", filename);
                    addFileToList(list, fullPath, dir.d_stat.st_size);
                }
            }
            sceIoDclose(dfd);
        }
    }

    if (cleanOrphanedData) {
        SceUID dfd = sceIoDopen("ux0:license/");
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (isEmergencyStopRequested()) break;
                if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
                if (strlen(dir.d_name) != 16 || isTitleIdInstalled(dir.d_name + 7)) continue;
                if (!isOlderThanDays(&dir.d_stat.st_mtime, 30)) continue;

                char fullPath[MAX_PATH_LENGTH];
                safe_snprintf(fullPath, sizeof(fullPath), "ux0:license/%s", dir.d_name);
                scanPathForPreview(list, fullPath);
            }
            sceIoDclose(dfd);
        }

        dfd = sceIoDopen("ux0:patch/");
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (isEmergencyStopRequested()) break;
                if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
                if (!safetyIsValidTitleId(dir.d_name) || isTitleIdInstalled(dir.d_name)) continue;
                if (!isOlderThanDays(&dir.d_stat.st_mtime, 30)) continue;

                char fullPath[MAX_PATH_LENGTH];
                safe_snprintf(fullPath, sizeof(fullPath), "ux0:patch/%s", dir.d_name);
                scanPathForPreview(list, fullPath);
            }
            sceIoDclose(dfd);
        }
    }
}

static int isTempFile(const char *filename) {
    if (!filename) return 0;

    int len = (int)strlen(filename);
    if (len == 0) return 0;

    const char *tempExtensions[] = {".tmp", ".temp", ".log", ".cache", ".bak", ".old", ".swp"};
    int extCount = sizeof(tempExtensions) / sizeof(tempExtensions[0]);

    for (int i = 0; i < extCount; i++) {
        int extLen = (int)strlen(tempExtensions[i]);
        if (len >= extLen && strcasecmp(filename + len - extLen, tempExtensions[i]) == 0) {
            return 1;
        }
    }

    if (strstr(filename, "temp") != NULL || strstr(filename, "tmp") != NULL ||
        strstr(filename, "cache") != NULL || strstr(filename, "log") != NULL) {
        return 1;
    }

    return 0;
}

static const char *APP_SCAN_DIRS[] = {
    "ux0:data/%s",
    "ux0:patch/%s",
    "ux0:addcont/%s",
    "ux0:rePatch/%s",
    "ux0:reAddcont/%s",
    "uma0:data/%s",
    "imc0:data/%s"
};

void scanAppTempFilesForPreview(FileList *list, const char *titleId) {
    if (!list || !titleId || strlen(titleId) != 9) return;
    if (whitelistCheck(NULL, titleId)) return;

    const char *tempDirs[] = {"cache", "temp", "tmp", "logs", "log"};
    int tempDirCount = sizeof(tempDirs) / sizeof(tempDirs[0]);
    int appDirCount = sizeof(APP_SCAN_DIRS) / sizeof(APP_SCAN_DIRS[0]);

    for (int dirIdx = 0; dirIdx < appDirCount; dirIdx++) {
        char appPath[MAX_PATH_LENGTH];
        safe_snprintf(appPath, sizeof(appPath), APP_SCAN_DIRS[dirIdx], titleId);

        SceIoStat stat;
        if (sceIoGetstat(appPath, &stat) < 0) continue;
        if (!SCE_S_ISDIR(stat.st_mode)) continue;

        for (int i = 0; i < tempDirCount; i++) {
            char tempDirPath[MAX_PATH_LENGTH];
            safe_snprintf(tempDirPath, sizeof(tempDirPath), "%s/%s", appPath, tempDirs[i]);
            scanPathForPreview(list, tempDirPath);
        }

        SceUID dfd = sceIoDopen(appPath);
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

                int skipDir = 0;
                for (int i = 0; i < tempDirCount; i++) {
                    if (strcmp(dir.d_name, tempDirs[i]) == 0) {
                        skipDir = 1;
                        break;
                    }
                }
                if (skipDir) continue;

                if (!SCE_S_ISDIR(dir.d_stat.st_mode) && isTempFile(dir.d_name)) {
                    char fullPath[MAX_PATH_LENGTH];
                    safe_snprintf(fullPath, sizeof(fullPath), "%s/%s", appPath, dir.d_name);
                    addFileToList(list, fullPath, dir.d_stat.st_size);
                }
            }
            sceIoDclose(dfd);
        }
    }
}

unsigned long long calculateAllAppsTempFilesSize() {
    if (!cleanAllAppsTempFiles) return 0;

    unsigned long long total = 0;
    char **apps = NULL;
    int appCount = 0;
    getInstalledAppsList(&apps, &appCount);

    if (!apps) return 0;

    for (int i = 0; i < appCount; i++) {
        if (!isEmergencyStopRequested()) {
            total += calculateSingleAppTempFilesSize(apps[i]);
        }
        free(apps[i]);
    }
    free(apps);

    return total;
}

unsigned long long cleanAllAppsTempFilesData() {
    if (!cleanAllAppsTempFiles) return 0;

    unsigned long long totalCleaned = 0;
    char **apps = NULL;
    int appCount = 0;
    getInstalledAppsList(&apps, &appCount);

    if (!apps) return 0;

    for (int i = 0; i < appCount; i++) {
        if (!isEmergencyStopRequested()) {
            totalCleaned += cleanSingleAppTempFiles(apps[i]);
        }
        free(apps[i]);
    }
    free(apps);

    return totalCleaned;
}

AppList* createAppList() {
    AppList *list = (AppList*)malloc(sizeof(AppList));
    if (!list) return NULL;

    list->capacity = 50;
    list->count = 0;
    list->apps = (AppInfo*)malloc(sizeof(AppInfo) * list->capacity);

    if (!list->apps) {
        free(list);
        return NULL;
    }

    return list;
}

void freeAppList(AppList *list) {
    if (list) {
        if (list->apps) {
            free(list->apps);
        }
        free(list);
    }
}

unsigned long long calculateSingleAppTempFilesSize(const char *titleId) {
    if (!titleId || strlen(titleId) != 9) return 0;
    if (whitelistCheck(NULL, titleId)) return 0;

    unsigned long long total = 0;
    const char *tempDirs[] = {"cache", "temp", "tmp", "logs", "log"};
    int tempDirCount = sizeof(tempDirs) / sizeof(tempDirs[0]);
    int appDirCount = sizeof(APP_SCAN_DIRS) / sizeof(APP_SCAN_DIRS[0]);

    for (int dirIdx = 0; dirIdx < appDirCount; dirIdx++) {
        char appPath[MAX_PATH_LENGTH];
        safe_snprintf(appPath, sizeof(appPath), APP_SCAN_DIRS[dirIdx], titleId);

        SceIoStat stat;
        if (sceIoGetstat(appPath, &stat) < 0) continue;
        if (!SCE_S_ISDIR(stat.st_mode)) continue;

        for (int j = 0; j < tempDirCount; j++) {
            char tempDirPath[MAX_PATH_LENGTH];
            safe_snprintf(tempDirPath, sizeof(tempDirPath), "%s/%s", appPath, tempDirs[j]);
            total += calculateTempSizeRecursive(tempDirPath);
        }

        SceUID dfd = sceIoDopen(appPath);
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

                int skipDir = 0;
                for (int j = 0; j < tempDirCount; j++) {
                    if (strcmp(dir.d_name, tempDirs[j]) == 0) {
                        skipDir = 1;
                        break;
                    }
                }
                if (skipDir) continue;

                if (!SCE_S_ISDIR(dir.d_stat.st_mode) && isTempFile(dir.d_name)) {
                    total += dir.d_stat.st_size;
                }
            }
            sceIoDclose(dfd);
        }
    }

    return total;
}

static int compareAppsByName(const void *a, const void *b) {
    const AppInfo *aa = (const AppInfo *)a;
    const AppInfo *ab = (const AppInfo *)b;
    const char *na = aa->appName[0] ? aa->appName : aa->titleId;
    const char *nb = ab->appName[0] ? ab->appName : ab->titleId;
    return strcasecmp(na, nb);
}

static int compareAppsBySize(const void *a, const void *b) {
    const AppInfo *aa = (const AppInfo *)a;
    const AppInfo *ab = (const AppInfo *)b;
    if (aa->tempSize > ab->tempSize) return -1;
    if (aa->tempSize < ab->tempSize) return 1;
    return compareAppsByName(a, b);
}

void sortAppList(AppList *list, SortMode sortMode) {
    if (!list || list->count <= 1) return;
    if (sortMode == SORT_BY_SIZE) {
        qsort(list->apps, (size_t)list->count, sizeof(AppInfo), compareAppsBySize);
    } else {
        qsort(list->apps, (size_t)list->count, sizeof(AppInfo), compareAppsByName);
    }
}

void populateAppListWithSizes(AppList *list) {
    if (!list) return;

    char **apps = NULL;
    int appCount = 0;
    getInstalledAppsList(&apps, &appCount);

    if (!apps) return;

    list->count = 0;

    for (int i = 0; i < appCount; i++) {
        if (list->count >= list->capacity) {
            list->capacity *= 2;
            AppInfo *newApps = (AppInfo*)realloc(list->apps, sizeof(AppInfo) * list->capacity);
            if (!newApps) {
                free(apps[i]);
                continue;
            }
            list->apps = newApps;
        }

        safe_strncpy(list->apps[list->count].titleId, apps[i], sizeof(list->apps[list->count].titleId));
        resolveTitleName(apps[i], list->apps[list->count].appName, sizeof(list->apps[list->count].appName));
        list->apps[list->count].tempSize = calculateSingleAppTempFilesSize(apps[i]);
        list->count++;

        free(apps[i]);
    }
    free(apps);

    sortAppList(list, SORT_BY_SIZE);
}

unsigned long long cleanSingleAppTempFiles(const char *titleId) {
    if (!titleId || strlen(titleId) != 9) return 0;
    if (whitelistCheck(NULL, titleId)) return 0;

    unsigned long long totalCleaned = 0;
    const char *tempDirs[] = {"cache", "temp", "tmp", "logs", "log"};
    int tempDirCount = sizeof(tempDirs) / sizeof(tempDirs[0]);
    int appDirCount = sizeof(APP_SCAN_DIRS) / sizeof(APP_SCAN_DIRS[0]);

    for (int dirIdx = 0; dirIdx < appDirCount; dirIdx++) {
        if (isEmergencyStopRequested()) break;

        char appPath[MAX_PATH_LENGTH];
        safe_snprintf(appPath, sizeof(appPath), APP_SCAN_DIRS[dirIdx], titleId);

        SceIoStat stat;
        if (sceIoGetstat(appPath, &stat) < 0) continue;
        if (!SCE_S_ISDIR(stat.st_mode)) continue;

        for (int j = 0; j < tempDirCount; j++) {
            if (isEmergencyStopRequested()) break;

            char tempDirPath[MAX_PATH_LENGTH];
            safe_snprintf(tempDirPath, sizeof(tempDirPath), "%s/%s", appPath, tempDirs[j]);

            unsigned long long before = calculateTempSizeRecursive(tempDirPath);
            deleteRecursive(tempDirPath);
            unsigned long long after = calculateTempSizeRecursive(tempDirPath);
            if (before > after) totalCleaned += (before - after);
        }

        SceUID dfd = sceIoDopen(appPath);
        if (dfd >= 0) {
            SceIoDirent dir;
            memset(&dir, 0, sizeof(SceIoDirent));

            while (sceIoDread(dfd, &dir) > 0) {
                if (isEmergencyStopRequested()) break;

                if (strcmp(dir.d_name, ".") == 0 || strcmp(dir.d_name, "..") == 0) continue;

                int skipDir = 0;
                for (int j = 0; j < tempDirCount; j++) {
                    if (strcmp(dir.d_name, tempDirs[j]) == 0) {
                        skipDir = 1;
                        break;
                    }
                }
                if (skipDir) continue;

                if (!SCE_S_ISDIR(dir.d_stat.st_mode) && isTempFile(dir.d_name)) {
                    char fullPath[MAX_PATH_LENGTH];
                    safe_snprintf(fullPath, sizeof(fullPath), "%s/%s", appPath, dir.d_name);

                    if (!whitelistCheck(fullPath, titleId) && sceIoRemove(fullPath) >= 0) {
                        totalCleaned += dir.d_stat.st_size;
                        g_deletedFilesCount++;
                    }
                }
            }
            sceIoDclose(dfd);
        }
    }

    return totalCleaned;
}

unsigned long long calculateOrphanedDLCDataSize() {
    unsigned long long total = 0;
    if (!cleanOrphanedDLC) return 0;

    SceUID dfd = sceIoDopen("ux0:addcont/");
    if (dfd < 0) return 0;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char dirName[MAX_FILENAME_LENGTH];
        safe_strncpy(dirName, dir.d_name, sizeof(dirName));

        if (safetyIsValidTitleId(dirName) && !isTitleIdInstalled(dirName)) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:addcont/%s", dirName);
            if (!whitelistCheck(fullPath, dirName)) {
                total += calculateTempSizeRecursive(fullPath);
            }
        }
    }
    sceIoDclose(dfd);

    return total;
}

void findOrphanedDLCData() {
    if (!cleanOrphanedDLC) return;

    SceUID dfd = sceIoDopen("ux0:addcont/");
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char dirName[MAX_FILENAME_LENGTH];
        safe_strncpy(dirName, dir.d_name, sizeof(dirName));

        if (safetyIsValidTitleId(dirName) && !isTitleIdInstalled(dirName)) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:addcont/%s", dirName);
            if (!whitelistCheck(fullPath, dirName)) {
                deleteRecursiveInternal(fullPath, 1, dirName);
            }
        }
    }
    sceIoDclose(dfd);
}

unsigned long long calculateOrphanedAddcontSize() {
    unsigned long long total = 0;
    if (!cleanOrphanedAddcont) return 0;

    SceUID dfd = sceIoDopen("ux0:addcont/");
    if (dfd < 0) return 0;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char dirName[MAX_FILENAME_LENGTH];
        safe_strncpy(dirName, dir.d_name, sizeof(dirName));

        if (strlen(dirName) != 9) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:addcont/%s", dirName);
            if (!whitelistCheck(fullPath, NULL)) {
                total += calculateTempSizeRecursive(fullPath);
            }
        }
    }
    sceIoDclose(dfd);

    return total;
}

void findOrphanedAddcont() {
    if (!cleanOrphanedAddcont) return;

    SceUID dfd = sceIoDopen("ux0:addcont/");
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char dirName[MAX_FILENAME_LENGTH];
        safe_strncpy(dirName, dir.d_name, sizeof(dirName));

        if (strlen(dirName) != 9) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:addcont/%s", dirName);
            if (!whitelistCheck(fullPath, NULL)) {
                deleteRecursiveInternal(fullPath, 1, NULL);
            }
        }
    }
    sceIoDclose(dfd);
}

unsigned long long calculateOrphanedLicenseFilesSize() {
    unsigned long long total = 0;
    if (!cleanOrphanedLicenseFiles) return 0;

    SceUID dfd = sceIoDopen("ux0:license/");
    if (dfd < 0) return 0;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

        char* filename = dir.d_name;
        int len = (int)strlen(filename);

        if (len > 4 && strcasecmp(filename + len - 4, ".rif") == 0) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:license/%s", filename);
            if (whitelistCheck(fullPath, NULL)) continue;

            int isOrphaned = 0;

            if (len == 20) {
                char titleId[10] = {0};
                memcpy(titleId, filename, 9);
                titleId[9] = '\0';
                if (!isTitleIdInstalled(titleId)) {
                    isOrphaned = 1;
                }
            } else if (len == 16 && isOlderThanDays(&dir.d_stat.st_mtime, 60)) {
                isOrphaned = 1;
            }

            if (isOrphaned) {
                total += dir.d_stat.st_size;
            }
        }
    }
    sceIoDclose(dfd);

    return total;
}

void findOrphanedLicenseFiles() {
    SceUID dfd = sceIoDopen("ux0:license/");
    if (dfd < 0) return;

    SceIoDirent dir;
    memset(&dir, 0, sizeof(SceIoDirent));

    while (sceIoDread(dfd, &dir) > 0) {
        if (isEmergencyStopRequested()) break;
        if (SCE_S_ISDIR(dir.d_stat.st_mode)) continue;
        char* filename = dir.d_name;
        int len = (int)strlen(filename);

        if (len > 4 && strcasecmp(filename + len - 4, ".rif") == 0) {
            char fullPath[MAX_PATH_LENGTH];
            safe_snprintf(fullPath, sizeof(fullPath), "ux0:license/%s", filename);
            if (whitelistCheck(fullPath, NULL)) continue;

            int isOrphaned = 0;

            if (len == 20) {
                char titleId[10] = {0};
                memcpy(titleId, filename, 9);
                titleId[9] = '\0';
                if (!isTitleIdInstalled(titleId)) {
                    isOrphaned = 1;
                }
            } else if (len == 16 && isOlderThanDays(&dir.d_stat.st_mtime, 60)) {
                isOrphaned = 1;
            }

            if (isOrphaned) {
                for (int attempt = 0; attempt < 3; attempt++) {
                    if (sceIoRemove(fullPath) >= 0) {
                        g_deletedFilesCount++;
                        break;
                    }
                    sceKernelDelayThread(100 * 1000);
                }
            }
        }
    }
    sceIoDclose(dfd);
}

unsigned long long calculateEmptyLiveareaBubblesSize() {
    return 0;
}

void removeEmptyLiveareaBubbles() {
    if (!cleanEmptyLiveareaBubbles) return;

    const char* bubbleDirs[] = {
        "ux0:app/",
        "ux0:patch/",
        "ux0:addcont/"
    };
    int bubbleDirCount = sizeof(bubbleDirs) / sizeof(bubbleDirs[0]);

    for (int d = 0; d < bubbleDirCount; d++) {
        if (isEmergencyStopRequested()) break;
        SceUID dfd = sceIoDopen(bubbleDirs[d]);
        if (dfd < 0) continue;

        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));

        while (sceIoDread(dfd, &dir) > 0) {
            if (isEmergencyStopRequested()) break;
            if (!SCE_S_ISDIR(dir.d_stat.st_mode)) continue;

            char dirName[MAX_FILENAME_LENGTH];
            safe_strncpy(dirName, dir.d_name, sizeof(dirName));

            if (safetyIsValidTitleId(dirName) && !isTitleIdInstalled(dirName)) {
                char fullPath[MAX_PATH_LENGTH];
                safe_snprintf(fullPath, sizeof(fullPath), "%s%s", bubbleDirs[d], dirName);
                if (whitelistCheck(fullPath, dirName)) continue;

                SceUID subDfd = sceIoDopen(fullPath);
                if (subDfd >= 0) {
                    int hasContent = 0;
                    SceIoDirent subDir;
                    memset(&subDir, 0, sizeof(SceIoDirent));

                    while (sceIoDread(subDfd, &subDir) > 0) {
                        if (strcmp(subDir.d_name, ".") != 0 && strcmp(subDir.d_name, "..") != 0) {
                            hasContent = 1;
                            break;
                        }
                    }
                    sceIoDclose(subDfd);

                    if (!hasContent) {
                        sceIoRmdir(fullPath);
                        g_deletedFilesCount++;
                    }
                }
            }
        }
        sceIoDclose(dfd);
    }
}

#define SETTINGS_FILE_PATH "ux0:data/PSV_Cleaner/settings.bin"
#define SETTINGS_VERSION_V1 1
#define SETTINGS_VERSION 2

typedef struct {
    int version;
    int excludePictureFolder;
    int excludeVpkFiles;
    int excludeVitaDBCache;
    int excludeVideoFolder;
    int cleanVitaShell;
    int cleanRetroArch;
    int cleanAdrenaline;
    int cleanBrowser;
    int cleanSystem;
    int cleanOrphanedData;
    int cleanAllAppsTempFiles;
    int cleanPkgi;
    int cleanAutoplugin;
    int cleanCrashDumps;
    int cleanEasyVpK;
    int cleanDaemon;
    int cleanVitaGrafix;
    int cleanOnemenu;
    int cleanPCSX;
    int cleanMGBA;
    int cleanFlycast;
    int cleanShellbat;
    int cleanSwitchUser;
    int cleanITLS;
    int cleanVHBB;
    int cleanPSVitaDB;
    int cleanDownloadEnabler;
    int cleanMoonlight;
    int cleanRetroFlow;
    int cleanHenkaku;
    int cleanPSVshell;
    int cleanCheatTools;
    int cleanThemeCache;
    int cleanNotificationCache;
    int cleanActivityLog;
    int cleanPhotoMusicCache;
    int cleanSceShellCache;
    int cleanFontCache;
    int cleanRegistryTemp;
    int cleanNetworkCache;
    int cleanLicenseCache;
    int cleanOrphanedLicenseFiles;
    int cleanOrphanedDLC;
    int cleanOrphanedAddcont;
    int cleanEmptyLiveareaBubbles;
} PersistentSettingsV1;

typedef struct {
    PersistentSettingsV1 base;
    int cleanRePatchOrphans;
    int currentTheme;
    int languageMode;
} PersistentSettings;

static void applySettingsV1(const PersistentSettingsV1 *s) {
    excludePictureFolder = s->excludePictureFolder;
    excludeVpkFiles = s->excludeVpkFiles;
    excludeVitaDBCache = s->excludeVitaDBCache;
    excludeVideoFolder = s->excludeVideoFolder;
    cleanVitaShell = s->cleanVitaShell;
    cleanRetroArch = s->cleanRetroArch;
    cleanAdrenaline = s->cleanAdrenaline;
    cleanBrowser = s->cleanBrowser;
    cleanSystem = s->cleanSystem;
    cleanOrphanedData = s->cleanOrphanedData;
    cleanAllAppsTempFiles = s->cleanAllAppsTempFiles;
    cleanPkgi = s->cleanPkgi;
    cleanAutoplugin = s->cleanAutoplugin;
    cleanCrashDumps = s->cleanCrashDumps;
    cleanEasyVpK = s->cleanEasyVpK;
    cleanDaemon = s->cleanDaemon;
    cleanVitaGrafix = s->cleanVitaGrafix;
    cleanOnemenu = s->cleanOnemenu;
    cleanPCSX = s->cleanPCSX;
    cleanMGBA = s->cleanMGBA;
    cleanFlycast = s->cleanFlycast;
    cleanShellbat = s->cleanShellbat;
    cleanSwitchUser = s->cleanSwitchUser;
    cleanITLS = s->cleanITLS;
    cleanVHBB = s->cleanVHBB;
    cleanPSVitaDB = s->cleanPSVitaDB;
    cleanDownloadEnabler = s->cleanDownloadEnabler;
    cleanMoonlight = s->cleanMoonlight;
    cleanRetroFlow = s->cleanRetroFlow;
    cleanHenkaku = s->cleanHenkaku;
    cleanPSVshell = s->cleanPSVshell;
    cleanCheatTools = s->cleanCheatTools;
    cleanThemeCache = s->cleanThemeCache;
    cleanNotificationCache = s->cleanNotificationCache;
    cleanActivityLog = s->cleanActivityLog;
    cleanPhotoMusicCache = s->cleanPhotoMusicCache;
    cleanSceShellCache = s->cleanSceShellCache;
    cleanFontCache = s->cleanFontCache;
    cleanRegistryTemp = s->cleanRegistryTemp;
    cleanNetworkCache = s->cleanNetworkCache;
    cleanLicenseCache = s->cleanLicenseCache;
    cleanOrphanedLicenseFiles = s->cleanOrphanedLicenseFiles;
    cleanOrphanedDLC = s->cleanOrphanedDLC;
    cleanOrphanedAddcont = s->cleanOrphanedAddcont;
    cleanEmptyLiveareaBubbles = s->cleanEmptyLiveareaBubbles;
}

void loadSettings() {
    PersistentSettings s;
    memset(&s, 0, sizeof(s));
    SceUID fd = sceIoOpen(SETTINGS_FILE_PATH, SCE_O_RDONLY, 0777);
    if (fd < 0) return;

    int readSize = sceIoRead(fd, &s, sizeof(PersistentSettings));
    sceIoClose(fd);

    if (readSize == (int)sizeof(PersistentSettingsV1) && s.base.version == SETTINGS_VERSION_V1) {
        applySettingsV1(&s.base);
        return;
    }

    if (readSize == (int)sizeof(PersistentSettings) && s.base.version == SETTINGS_VERSION) {
        applySettingsV1(&s.base);
        cleanRePatchOrphans = s.cleanRePatchOrphans;
        setTheme((AppTheme)s.currentTheme);
        setLanguageMode(s.languageMode);
    }
}

void saveSettings() {
    PersistentSettings s;
    memset(&s, 0, sizeof(PersistentSettings));

    s.base.version = SETTINGS_VERSION;
    s.base.excludePictureFolder = excludePictureFolder;
    s.base.excludeVpkFiles = excludeVpkFiles;
    s.base.excludeVitaDBCache = excludeVitaDBCache;
    s.base.excludeVideoFolder = excludeVideoFolder;
    s.base.cleanVitaShell = cleanVitaShell;
    s.base.cleanRetroArch = cleanRetroArch;
    s.base.cleanAdrenaline = cleanAdrenaline;
    s.base.cleanBrowser = cleanBrowser;
    s.base.cleanSystem = cleanSystem;
    s.base.cleanOrphanedData = cleanOrphanedData;
    s.base.cleanAllAppsTempFiles = cleanAllAppsTempFiles;
    s.base.cleanPkgi = cleanPkgi;
    s.base.cleanAutoplugin = cleanAutoplugin;
    s.base.cleanCrashDumps = cleanCrashDumps;
    s.base.cleanEasyVpK = cleanEasyVpK;
    s.base.cleanDaemon = cleanDaemon;
    s.base.cleanVitaGrafix = cleanVitaGrafix;
    s.base.cleanOnemenu = cleanOnemenu;
    s.base.cleanPCSX = cleanPCSX;
    s.base.cleanMGBA = cleanMGBA;
    s.base.cleanFlycast = cleanFlycast;
    s.base.cleanShellbat = cleanShellbat;
    s.base.cleanSwitchUser = cleanSwitchUser;
    s.base.cleanITLS = cleanITLS;
    s.base.cleanVHBB = cleanVHBB;
    s.base.cleanPSVitaDB = cleanPSVitaDB;
    s.base.cleanDownloadEnabler = cleanDownloadEnabler;
    s.base.cleanMoonlight = cleanMoonlight;
    s.base.cleanRetroFlow = cleanRetroFlow;
    s.base.cleanHenkaku = cleanHenkaku;
    s.base.cleanPSVshell = cleanPSVshell;
    s.base.cleanCheatTools = cleanCheatTools;
    s.base.cleanThemeCache = cleanThemeCache;
    s.base.cleanNotificationCache = cleanNotificationCache;
    s.base.cleanActivityLog = cleanActivityLog;
    s.base.cleanPhotoMusicCache = cleanPhotoMusicCache;
    s.base.cleanSceShellCache = cleanSceShellCache;
    s.base.cleanFontCache = cleanFontCache;
    s.base.cleanRegistryTemp = cleanRegistryTemp;
    s.base.cleanNetworkCache = cleanNetworkCache;
    s.base.cleanLicenseCache = cleanLicenseCache;
    s.base.cleanOrphanedLicenseFiles = cleanOrphanedLicenseFiles;
    s.base.cleanOrphanedDLC = cleanOrphanedDLC;
    s.base.cleanOrphanedAddcont = cleanOrphanedAddcont;
    s.base.cleanEmptyLiveareaBubbles = cleanEmptyLiveareaBubbles;
    s.cleanRePatchOrphans = cleanRePatchOrphans;
    s.currentTheme = (int)currentTheme;
    s.languageMode = languageMode;

    sceIoMkdir("ux0:data/PSV_Cleaner", 0777);
    SceUID fd = sceIoOpen(SETTINGS_FILE_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, &s, sizeof(PersistentSettings));
        sceIoClose(fd);
    }
}

volatile int g_bgTask = BG_TASK_IDLE;
volatile int g_bgTaskDone = 1;
FileList *g_bgPreviewList = NULL;
AppList *g_bgAppList = NULL;
char g_bgCleanAppTitleId[16] = "";
volatile unsigned long long g_bgSpaceFreed = 0;
SortMode g_bgSortMode = SORT_BY_NAME;
char g_bgFileFilter[MAX_FILE_FILTER_LENGTH] = "";
char g_bgSearchQuery[64] = "";
int g_bgCleanSubsetMode = 0;
unsigned long long g_bgVisibleSize = 0;
char g_bgTargetPath[MAX_PATH_LENGTH] = "";
char g_bgTargetTitleId[16] = "";
int g_bgLargeMinMB = 50;
int g_bgLastActionOk = 0;
char g_bgLastActionMsg[128] = "";
char g_bgActiveModeLabel[32] = "Clean";

static volatile int g_bgRunning = 0;
static SceUID g_bgThreadId = -1;

static int bgWorkerThread(SceSize argc, void *argp) {
    (void)argc;
    (void)argp;
    while (g_bgRunning) {
        if (g_bgTask == BG_TASK_CALC_SIZE) {
            g_cachedSpaceSize = calculateTempSize();
            g_spaceCalculationNeeded = 0;
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_SCAN_PREVIEW) {
            if (g_bgPreviewList) {
                scanFilesForPreview(g_bgPreviewList);
                filterAndSortFileList(g_bgPreviewList, g_bgSortMode, g_bgFileFilter, &g_bgVisibleSize);
            }
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_SCAN_APPS) {
            if (g_bgAppList) {
                populateAppListWithSizes(g_bgAppList);
            }
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_CLEAN) {
            unsigned long long freed = cleanTemporaryFiles();
            historyAppend(g_bgActiveModeLabel, getDeletedFilesCount(), 0, isEmergencyStopRequested(), freed);
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_CLEAN_PREVIEW_SUBSET) {
            unsigned long long freed = 0;
            if (g_bgCleanSubsetMode == 1) {
                freed = cleanSelectedPreviewFiles(g_bgPreviewList);
            } else {
                freed = cleanFilteredPreviewFiles(g_bgPreviewList, g_bgFileFilter, g_bgSearchQuery);
            }
            historyAppend(g_bgActiveModeLabel, getDeletedFilesCount(), 0, isEmergencyStopRequested(), freed);
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_CLEAN_APP) {
            g_bgSpaceFreed = cleanSingleAppTempFiles(g_bgCleanAppTitleId);
            historyAppend("Per-App Clean", getDeletedFilesCount(), 0, isEmergencyStopRequested(), g_bgSpaceFreed);
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_ANALYZE_VOLUME) {
            analyzerScanVolume(g_analyzerActiveVolume);
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_ANALYZE_DIR) {
            analyzerBrowseDir(g_bgTargetPath);
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_SMART_SCAN) {
            smartCleanScan();
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_SMART_CLEAN) {
            startOperation();
            smartCleanRun();
            historyAppend("Smart Clean", (int)g_cleanReport.filesDeleted, (int)g_cleanReport.filesFailed, isEmergencyStopRequested(), g_cleanReport.bytesRecovered);
            endOperation();
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_ORPHAN_SCAN) {
            orphanScanRun();
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_ORPHAN_DELETE) {
            startOperation();
            orphanDeleteSelected();
            historyAppend("Orphan Clean", (int)g_cleanReport.filesDeleted, (int)g_cleanReport.filesFailed, isEmergencyStopRequested(), g_cleanReport.bytesRecovered);
            endOperation();
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_LARGE_SCAN) {
            largeFilesScanRun(g_bgLargeMinMB);
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_LARGE_DELETE) {
            startOperation();
            largeFilesDeleteSelected();
            endOperation();
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_TAI_AUDIT) {
            taiAuditRun();
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        } else if (g_bgTask == BG_TASK_BACKUP_SAVE || g_bgTask == BG_TASK_BACKUP_DELETE_SAVE) {
            int del = (g_bgTask == BG_TASK_BACKUP_DELETE_SAVE) ? 1 : 0;
            g_bgLastActionOk = backupOrphanSaveZip(g_bgTargetTitleId, g_bgTargetPath, del, g_bgLastActionMsg, sizeof(g_bgLastActionMsg));
            if (g_bgLastActionOk && del) {
                orphanScanRun();
                invalidateSpaceCache();
            }
            g_bgTask = BG_TASK_IDLE;
            g_bgTaskDone = 1;
        }
        sceKernelDelayThread(8 * 1000);
    }
    sceKernelExitThread(0);
    return 0;
}

void startBgWorker() {
    if (g_bgThreadId >= 0) return;
    maintenanceInit();
    g_bgRunning = 1;
    g_bgThreadId = sceKernelCreateThread("psv_cleaner_bg", bgWorkerThread, 0x10000101, 0x40000, 0, 0, NULL);
    if (g_bgThreadId >= 0) {
        sceKernelStartThread(g_bgThreadId, 0, NULL);
    } else {
        g_bgRunning = 0;
    }
}

void stopBgWorker() {
    if (g_bgThreadId < 0) return;
    g_bgRunning = 0;
    requestEmergencyStop();
    sceKernelWaitThreadEnd(g_bgThreadId, NULL, NULL);
    sceKernelDeleteThread(g_bgThreadId);
    g_bgThreadId = -1;
    g_emergencyStop = 0;
}

void requestBgTask(BgTask task) {
    if (!g_bgRunning || !g_bgTaskDone) return;
    g_bgTaskDone = 0;
    g_bgTask = task;
}

int isBgBusy() {
    return g_bgRunning && !g_bgTaskDone;
}

void waitBgIdle() {
    while (g_bgRunning && !g_bgTaskDone) {
        sceKernelDelayThread(10 * 1000);
    }
}
