******************************************************************************
                                  Announcing

                            APM/2 Version 1.40 for
                               IBM OS/2 Warp 4
       Copyright (C) by Roman Stangl (rstangl@vnet.ibm.com) 1997, 1998

******************************************************************************

Dear User, Lieber Benutzer!

APM/2 allows you to put your workstation into APM standby, suspend or power 
off mode under Warp 4. I emphasize Warp 4, as only that version has the
required support in APM.SYS, other system files and the Kernel to successfully
process APM power off requests. However, you can force APM/2 power off to run 
under Warp 3 and before by the commandline option /Force (usefuly when a 
fixpack improves APM support). APM/2 also allows you to schedule a automatic
power on, if APM level 1.2+ is detected. APM requests can also be scheduled.
APM/2 is NLS enabled that allows you easily to create a translation in your
national (SBCS single byte character set) language.

Just launch the executable from an OS/2 window to see the commandline options.

Under previous versions of OS/2 for APM power off requests, the filesystem 
will not be closed correctly, causing a CHKDSK during the next reboot. And a
tip from Frank Schroeder (OS/2 I/O Subsystem/Device Driver Development): "Do 
not try to run the Version 4.0 APM device driver on OS/2 Warp Version 3.0 as 
there are dependencies upon other system files including the os2krnl".

Though Warp 4 is supposed to perform a filesystem shutdown when requesting APM
power off, you can force APM/2 to flush the filesystem before invoking APM
power off and from a user I know that this (surprisingly) increased stability.
There are 2 shutdown options, DosShutdown(0) shuts down and locks the file 
system (which causes any file I/O to fail or block) and DosShutdown(1) which
quiescent executing threads but does not lock the file system.

The sourcecode is available in the encrypted file NLS.ZIE. By using the
program PROTECT.EXE you can decrypt it into NLS.ZIP by entering from an OS/2
commandline:
PROTECT apm2v13 NLS.ZIE

By using encryption I can ensure that everyone interested viewing the source
code must have access to an OS/2 installation, as I'll only provide a 32-bit
OS/2 version of the decryption program (my answer to "Windows-only" vendors).

The archive NLS.ZIP contains the following files:
Apm.cpp ....... Source code
Apm.hpp ....... Source code header 
Apm.h ......... APM for OS/2 toolkit
Apm.def ....... Module definition file
ApmUs.txt ..... Message source file (English)
ApmGr.txt ..... Message source file (German)
Make.cmd ...... Batch file to build English and German versions

Please run the executables to read the conditions of APM/2 use!

You are welcome to drop me some comments or to visit my homepage at the URL:
http://www.geocities.com/SiliconValley/Pines/7885/

Warning! APM Power Off may not work on all systems (and I've also seen a WIN95
         OS2R2 preloaded PC that did hang after a APM power off request, so
         no reason to think it's just OS/2's fault) despite they are supporting 
         APM and APM.SYS works correctly. Symptoms are (thanks to Frank 
         Schroeder from the OS/2 I/O Subsystem/Device Driver Development, 
         who owns the OS/2 APM driver, for his support to give me some 
         explanations about possible causes):

         *) APM power off does power off your PC, but even under Warp 4 a
            CHKDSK is performed due to the next reboot. This may be related to 
            the combination of a too fast processor and a too slow harddisk, 
            because the request to the disk subsystem didn't finish in time 
            before the power is dropped (this may corrupt partitions!). 
         *) Your PC is powered off, but immediately powers on again. This may 
            be related that the APM BIOS thinks that is should power up again, 
            which can be caused by "Wakeup on LAN" adapters or that OS/2's APM 
            support polls APM BIOS which confuses APM BIOS.
         *) APM power off does not work under OS/2 but does under Windows. As 
            OS/2 (as a protected mode operating system) may use different 
            entrypoints into APM BIOS as Windows, there are some chances that 
            there are BIOS bugs. Even when the same BIOS code is used, timing 
            issues ad differences in the frequency of polling, the order of 
            APIs called, the method of APM event notifications,... can explain 
            differences.

Warm regards from Austria! Roman

******************************************************************************

APM/2 erlaubt es Ihnen, Ihren Rechner unter Warp 4 in den APM standby, suspend
oder power off Modus zu bringen. Die Betonung liegt auf Warp 4, da nur diese
Version APM Funktionen in APM.SYS, anderen Systemdateien und im Kernel
verarbeiten kann. Sie kînnen jedoch mit der Befehlszeilen Option /Force  APM/2 
power off auch unter Warp 3 und frÅher ausfÅhren (nÅtzlich fÅr den Fall, dass 
APM mit einem Fixpack besser unterstÅtzt wird). APM/2 erlaubt es auch einen
Zeitpunkt fÅr automatisches Einschalten zu planen, wenn APM level 1.2+ vor-
handen ist. Die APM Funktionen kînnen auch geplant werden. APM/2 ist NLS
fÑhig und erlaubt es ohne Probleme nationale (SBCS single byte character set)
Versionen zu Åbersetzen.

Rufen Sie das Programm einfach von einer OS/2 Befehlszeile auf um die
Optionen angezeigt zu bekommen

Unter frÅheren Versionen von OS/2 wird das Dateisystem bei APM power off
Funktionen nicht korrekt abgeschlossen, was zu einem CHKDSK beim nÑchsten
Hochfahren fÅhrt. Noch ein Tip von Frank Schroeder (OS/2 I/O Subsysteme/
GerÑtetreiber Entwicklung): "Versuchen Sie nicht den Version 4.0 APM GerÑte-
treiber unter OS/2 Warp Version 3.0 einzusetzen, da es auch AbhÑngigkeiten
von anderen Systemdateien inklusive dem os2krnl gibt".

Obwohl Warp 4 ein Shutdown des Dateisystems durchfÅhren sollte, kînnen Sie 
APM/2 zwingen ein Shutdown durchzufÅhren bevor APM power off aufgerufen wird.
Von einem Benutzer weiss ich, dass das (Åberraschenderweise) die StabilitÑt
erhîht hat. Es gibt dazu 2 Shutdown Optionen, DosShutdown(0) macht ein
Shutdown des Dateisystems und beendet es (wodurch weiteres Datei I/O nicht
mehr mîglich ist oder blokiert) und DosShutdown(1) was zwar the Threads
aber nicht Dateisystemzugriffe blokiert.

Der Quellenkode befindet sich verschlÅsselt in der Datei NLS.ZIE. Durch das
Programm PROTECT.EXE kann diese jedoch in die Datai NLS.ZIP dekodiert
werden, indem man auf einer OS/2 Befehlszeile eingibt:
PROTECT apm2v13 NLS.ZIE

Durch die VerschlÅsselung ist sichergestellt, dass jeder der an dem 
Quellenkode interessiert ist Zugriff auf eine OS/2 Installation haben muss,
da ich nur eine 32-bit OS/2 Version des EntschlÅsselungsprogramms zur
VerfÅgung stelle (meine Antwort auf "Windows-only" SoftwarehÑuser).

Das Archiv NLS.ZIP beinhaltet folgende Dateien:
Apm.cpp ....... Quellenkode
Apm.hpp ....... Quellenkode Header 
Apm.h ......... APM fÅr OS/2 Toolkit
Apm.def ....... Modul Definitionsdatei
ApmUs.txt ..... Nachrichtendatei Quellenkode (English)
ApmGr.txt ..... Nachrichtendatai Quellenkode (German)
Make.cmd ...... Stapeljob um die englische und deutsche Version zu erzeugen

Starten Sie bitte das Programm um die Benutzungsbedingungen zu lesen!

Kommentare sind gerne willkommen, oder auch ein Besuch auf meiner Homepage
unter der URL:
http://www.geocities.com/SiliconValley/Pines/7885/

Warnung! Es besteht die Mîglichkeit, dass APM Power Off nicht auf allen 
         Systemen korrekt funktioniert (ich habe auch schon ein WIN95 OS2R2
         preloaded System gesehen wo ein APM Power Off request zu einem
         kompletten Hang gefÅhrt hat), obwohl APM unterstÅtzt wird und 
         APM.SYS korrekt funktioniert. Symptome sind (Dank an Frank Schroeder
         vom OS/2 I/O Subsystem/Device Driver Development, der der Owner des
         OS/2 APM Treibers ist, fÅr seine UnterstÅtzung der Suche nach 
         mîglichen ErklÑrungen der Ursachen):
        
         *) APM power off schaltet Ihren PC aus, aber trotz Warp 4 wird beim
            nÑchsten Boot ein CHKDSK ausgefÅhrt. Mîgliche Ursache kann die
            Kombination eines schnellen Prozessors mit einer langsamen 
            Festplatte sein, da die Festplatte nicht rechtzeitig fertig 
            geworden ist bevor die Stromversorgung abgeschalten wurder (das
            kann auch Partitionen beschÑdigen).
         *) Ihr PC wird ausgeschalten, schaltet sich aber sofort wieder ein.
            Das kann damit zusammenhÑngen, dass das BIOS glaubt den PC wieder
            einschalten zu mÅssen, weil ein "Wakup on LAN" Adapter dies 
            verlangt oder dass der OS/2 APM Treiber das APM BIOS abfragt und
            damit verwirrt.
         *) APM power off funktioniert nicht unter OS/2 aber unter Windows.
            Da OS/2 (als protected mode Betriebssystem) mîglicherweise andere
            Einsprungspunkte in das APM BIOS benutzt als Windows, kann es 
            durch BIOS Fehler zu Unterschieden kommen. Selbst wenn der gleiche
            BIOS Kode benutzt wird, kînnen Unterschiede durch Timingprobleme,
            Unterschieder in der Abfragefrequenz, der Reihenfolge der 
            Funktionsaufrufe, der verwendeten Methode fÅr APM Notifikationen,
            ... werden. 

Mit freundlichen GrÅssen aus ôsterreich! Roman

