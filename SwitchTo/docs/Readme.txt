******************************************************************************
                                  Announcing

                          SwitchTo/2 Version 1.20 for
                                   IBM OS/2
          Copyright (C) by Roman Stangl (rstangl@vnet.ibm.com), 1998

******************************************************************************

Dear User, Lieber Benutzer!

SwitchTo/2 allows you to switch a session locateable in the Window into the
foreground from a commandline. For your info, I had the need to switch a
certain fullscreen OS/2 session into the foreground to get it the priority
boost the foreground session gets under OS/2 automatically (unless you have
specified PRIORITY=ABSOLUTE in your CONFIG.SYS) from STARTUP.CMD on an 
unattended workstation, where the Netfinity Systems Management Application
is the only interface to force daily reboots remotely. 

From a programmer's viewpoint, SwitchTo/2 shows how to call PM APIs from a 
non-PM session. Creating a message queue with WinCreateMsgQueue() fails with
and PMERR_NOT_IN_A_PM_SESSION if the session this API was called from is not
a PM session. The point is how a PM session is defined.

It is assumed widely that specifying the attribute WINDOWAPI in the module
definition file defines that a program will start in a PM session, or
WINDOWCOMPAT to start it in a AVIO session. This is true insofar, that
when the WPS (and likely other launchers) launches the program, it takes
these attribute from the executable file and creates a corresponding session
environment. However, you can force a AVIO session into a PM session
environment by overriding the attribute (e.g. by supplying the option /PM
when starting is from a commandline).

SwitchTo/2 simply does that when compiled with the macro LAUNCHCOPY defined. 
Once invoked from a commandline as a AVIO (commandline) session, it launches 
itself in a PM session again, from where it then can call any PM API (though 
just for enumerating the Window List and switching to a session no message 
queue is required and therefore it could also be done by just a AVIO session, 
enhancements requireing a message queue are easy to imagine). By using a 
termination queue the AVIO session ensures to receive the return code of the 
PM session. By the way, the JAVA interpreter does the same trick, as it can 
also launch non-AWT and AWT base applications from the same executable.

When compiled with the macro CHANGETYPE defined, SwitchTo/2 just changes its
application type, by overriding the value retrieved from the program info
block. That way PM APIs can be called and the output to STDOUT is still
visible when invoked from a command line.

This archive contains the following files:
SwitchTo.exe .. SwitchTo/2 executable
SwitchTo.msg .. SwitchTo/2 message file (renamed SW2Us.msg or SW2Gr.msg)
Read.me ....... This file
SwitchTo.cpp .. Source code
SwitchTo.hpp .. Source code
SwitchTo.def .. Source code
SW2Us.txt ..... English message file source
SW2Gr.txt ..... German message file source
SW2Us.msg ..... English message file
SW2Gr.msg ..... German message file
Makefile ...... Makefile

You may freely distribute/modify this program, provided the complete source
is included. 

You are welcome to drop me some comments or to visit my homepage at the URL:
http://www.geocities.com/SiliconValley/Pines/7885/

Warm regards from Austria! Roman

******************************************************************************

SwitchTo/2 erlaubt es Dir, eine in der Fensterliste befindliche Session von
einer Befehlszeile aus in the Vordergrund zu schalten. Zur Information, ich
hatte den Bedarf einen Gesamtbildschirm in den Vordergrund zu schalten, um 
die PrioritÑtserhîhung die OS/2 automatisch der Session im Vordergrund zu-
kommen lÑsst (ausser PRIORITY=ABSOLUTE ist in der CONFIG.SYS angegeben)
auszunutzen. Da es sich dabei um eine unbeaufsichtigte Workstation handelte,
die tÑglich remote durch das Netfinity Systems Management rebootet wird,
musste das in der STARTUP.CMD automatisiert werden.

Programmiertechnisch zeigt SwitchTo/2 wie man PM APIs aus einer nicht-PM
Session aufrufen kann. Die Erzeugung einer Message Queue mittels 
WinCreateMsgQueue() schlÑgt mit PMERR_NOT_IN_A_PM_SESSION fehl, wenn die 
Session von der der API aufgerufen wurde keine PM Session ist. Der Punkt ist
wie eine PM Session definiert wird.

Es wird weit verbreitet angenommen, dass die Angabe des Attributes WINDOWAPI
in der Module Definition Datei definiert, ob ein Programm in einer PM Session
gestartet wird, bzw. WINDOWCOMPAT um in eine AVIO Session zu starten. Das ist
insofern korrekt, als dass die WPS (und wahrscheinlich andere Programmstarter)
das Programm anhand dieses Attributes im Executable in die entsprechende 
Session starten. Aber, man kann den Start einer AVIO Session in eine PM
Session erzwingen, indem man das Attribut Åberschreibt (z.B. durch Angabe der
Option /PM in einer Befehlszeile). 

SwitchTo/2 macht genau das wenn beim Kompilieren das Makro LAUNCHCOPY 
definiert ist. Beim Aufruf von einer Befehlszeile wird es als AVIO 
(Befehlszeilen-) Session gestartet. Es startet sich dann jedoch selbst
nocheinmal in eine PM Session, von der es dann PM APIs aufrufen kann (obwohl
um die Fensterleiste zu enumerieren und zu einer Session zu schalten ist 
keine Message Queue nîtig und kînnte daher von einer AVIO Session gemacht
werden, es sind aber leicht Erweiterungen vorstellbar, die eine Message Queue
erfordern). Durch Verwendung einer Termination Queue stellt die AVIO Session
sicher, den RÅckgabecode der PM Session feststellen zu kînnen. öbrigends, der 
JAVA Interpreter benutzt den selben Trick, er kann dadurch sowohl nicht-AWT 
als auch AWT Applikationen durch das selbe Executable starten.

When compiled with the macro CHANGETYPE defined, SwitchTo/2 just changes its
application type, by overriding the value retrieved from the program info
block. That way PM APIs can be called and the output to STDOUT is still
visible when invoked from a command line.

Wenn beim Kompilieren das Makro CHANGETYPE definiert ist, Ñndert SwitchTo/2
einfach seinen Typ, indem der Wert vom Prozess Informations Block (PIB)
Åberschreiben wird. Dadurch kînnen ebenfalls PM APIs aufgerufen werden, und
die Ausgabe auf STDOUT ist dennoch sichtbar wenn das Programm von einer
Befehlszeile aufgerufen wurde.

Dieses Archive enthÑlt folgende Dateien:
SwitchTo.exe .. SwitchTo/2 Executable
SwitchTo.msg .. SwitchTo/2 Nachrichtendatei (umbenannte SW2Us.msg oder 
                SW2Gr.msg)
Read.me ....... Diese Datei
SwitchTo.cpp .. Quellencode
SwitchTo.hpp .. Quellencode
SwitchTo.def .. Quellencode
SW2Us.txt ..... Englische Nachrichtendatei Quellencode
SW2Gr.txt ..... Deutsche Nachrichtendatai Quellencode
SW2Us.msg ..... Englische Nachrichtendatei
SW2Gr.msg ..... Deutsche Nachrichtendatei
Makefile ...... Makefile

Du bist auch willkommen mir eine Nachricht zu schicken, oder meine Homepage
mit folgender URL zu besuchen:
http://www.geocities.com/SiliconValley/Pines/7885/

Du kannst dieses Programm frei verteilen/modifizieren, vorrausgesetzt der
komplette Quellencode wird inkludiert.

Mit freundlichen GrÅssen aus ôsterreich! Roman

