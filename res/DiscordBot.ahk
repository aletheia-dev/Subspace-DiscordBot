; ============================================================================
; DiscordBot v1.0 - by Aletheia
;
; A lean observer for connecting the Subspace chat to Discord.
; ============================================================================

#SingleInstance Ignore
#DllLoad "DiscordBot"

if !ProcessExist("Continuum.exe") {
    Run "Continuum"
    Sleep 8000
;    MsgBox "Continuum is not running!"
;    ExitApp
}

;MsgBox "Press Ctrl+r to run and Ctrl+q to quit.", "DiscordBot v1.4.2"

WinActivate "Continuum 0.40"

f1 := CallbackCreate((c, p) => %StrGet(c, "cp0")%(StrGet(p, "cp0")), "F")
f2 := CallbackCreate((p) => ControlSend(StrGet(p, "cp0"), , "OBS"), "F")
    
if DllCall("DiscordBot\start", "Ptr", f1, "Ptr", f2, "Int") {
    ; process log messages until the user quits the application
    loop {
        if !DllCall("DiscordBot\process", "Int") {
            ExitApp
        }
        Sleep 500
    }
}
else {
    ExitApp
}



; ----------------------------------------------------------------------------
; Run the bot. This hotkey has to be applied either in the game or while
; the Continuum chat window is in focus.
; ----------------------------------------------------------------------------
;^r::{
;    try {
;        WinActivate "Continuum 0.40"
;    }
;    catch {
;        ; we are in the game, nothing to do
;    }
;
;    f1 := CallbackCreate((c, p) => %StrGet(c, "cp0")%(StrGet(p, "cp0")), "F")
;    f2 := CallbackCreate((p) => ControlSend(StrGet(p, "cp0"), , "OBS"), "F")
;    
;    if DllCall("DiscordBot\start", "Ptr", f1, "Ptr", f2, "Int") {
;        ; process log messages until the user quits the application
;        loop {
;            DllCall "DiscordBot\process"
;            Sleep 500
;        }
;    }
;    else {
;        ExitApp
;    }
;}

; ----------------------------------------------------------------------------
; Force match recording. Used in cases where the bot is late to a match.
; ----------------------------------------------------------------------------
^p::{
    try {
        WinActivate "Continuum 0.40"
    }
    catch {
        ; we are in the game, nothing to do
    }

    f1 := CallbackCreate((c, p) => %StrGet(c, "cp0")%(StrGet(p, "cp0")), "F")
    f2 := CallbackCreate((p) => ControlSend(StrGet(p, "cp0"), , "OBS"), "F")
    
    if DllCall("DiscordBot\start", "Ptr", f1, "Ptr", f2, "Int") {
        DllCall "DiscordBot\force"
        ; process log messages until the user quits the application
        loop {
            if !DllCall("DiscordBot\process", "Int") {
                ExitApp
            }
            Sleep 500
        }
    }
    else {
        ExitApp
    }
}

; ----------------------------------------------------------------------------
; Quit the bot.
; ----------------------------------------------------------------------------
^q::{
    try {
        WinActivate "Continuum 0.40"
    }
    catch {
        ; we are in the game, nothing to do
    }
    DllCall "DiscordBot\stop"
    ExitApp
}

; ----------------------------------------------------------------------------
; Test helper.
; ----------------------------------------------------------------------------
^t::{
    try {
        WinActivate "Continuum 0.40"
    }
    catch {
        ; we are in the game, nothing to do
    }

    DllCall "DiscordBot\test"
}
