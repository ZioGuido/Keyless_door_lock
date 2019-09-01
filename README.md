# Keyless_door_lock
Arduino project to control a motorized door lock

(Scroll below for a short english description)
ITALIANO

Nell'aprile del 2017 ho fatto installare, alla porta d'ingresso del mio ufficio, una serratura motorizzata con annesso sistema di controllo 
prodotto da un'azienda di Treviso specializzata in questo tipo di serrature "keyless", ovvero che si possono usare senza chiavi, 
comandabili con un tastierino numerico oppure da remoto. Il sistema che ho fatto installare io consiste solo in una serratura 
opportunamente modificata per essere azionata da un motoriduttore, e una scheda di controllo che riceve comandi da un pulsante per 
comandare l'apertura, un interruttore per decidere la chiusura, e un sensore magnetico per intercettare lo stato aperta/chiusa della porta.
Ebbene, dopo poco più di due anni un violento temporale causa dei danni all'impianto di casa e questa centralina si danneggia insieme ad
altri impianti. Quando contatto la ditta per chiedere se fosse possibile una riparazione fuori garanzia oppure acquistare una centralina 
nuova, mi viene detto che il prodotto è obsoleto e bisogna acquistare il modello nuovo che costa il doppio di quello vecchio. 
Ovviamente, da questo rifiuto ne esco sconfitto e amareggiato, soprattutto perché da un'azienda che dice di produrre elettronica 
industriale e di essere specializzata in sicurezza, ci si aspetta la possibilità di un supporto a lungo termine, ma così non è stato.

Nell'ispezionare il prodotto, scopro che il cuore del sistema è una scheda clone di Arduino Duemilanove denominata "Pro Mini" basata su un
ATMega328P. Il circuito viene alimentato a 12V, e un regolatore 7805 genera i 5V che alimentano il microcontrollore; sulla scheda sono 
presenti un driver di potenza L6201P a due canali e un resistore di potenza da 3,9 Ohm che assorbe il carico del motore qualora questo 
andasse sotto sforzo. Il motoriduttore utilizzato per il progetto è un MICROMOTORS E192-2S.12.49 con encoder. Il danno provocato dal 
fulmine riguardava la rottura del controllore Atmel e i due Hall-IC presenti nell'encoder del motore. Dopo aver riparato la scheda, ho 
ricavato uno schema dei collegamenti e ho deciso di scrivere io stesso un nuovo programma che riproducesse le medesime funzioni del 
prodotto originale.

I collegamenti della scheda ProMini sono i seguenti:

PIN
12      L6201 In 1
11      L6201 In 2
10      L6201 Enable
3       Encoder A
5       Open Button
2       Door Sensor
4       Day&Night Switch
13      Jumper "No Release"
8       Jumper "Error yes"
6       Jumper "Reset"
7       LED "ok"

FUNZIONAMENTO

Alla prima accensione è necessario effettuare la taratura per conoscere le corse da effettuare per bloccare/sbloccare la porta o
aprire e rilasciare lo scrocco. Per far ciò, dopo aver montato la serratura e il motore ed effettuato i collegamenti, 
chiudere la porta e posizionare il chiavistello in posizione di porta chiusa (scrocco rilasciato) ma senza le mandate; 
prima di alimentare la scheda, rimuovere il jumper RESET, poi dare alimentazione e mantenere la porta chiusa. Il motore tenterà di 
aprire la porta, raggiunto il fine corsa, girerà in senso opposto per chiudere le mandate, poi si ferma. Togliere l'alimentazione, 
posizionare il jumper RESET e dare nuovamente alimentazione. 

Tramite il sensore magnetico (reed switch) CSA 401-TF è possibile sapere se la porta è aperta oppure chiusa. Quando la porta è chiusa
e la serratura è bloccata con le mandate, premendo il pulsante il motore girerà in senso di apertura e attenderà che la porta venga 
aperta. Solo dopo che la porta è stata aperta, verrà rilasciato lo scrocco. 
Quando si richiude la porta, se il selettore D&N è impostato su ON, la porta resta sbloccata; se si imposta D&N su off, la serratura 
farà le mandate dopo aver chiuso la porta.
Se si preme il pulsante di apertura porta e non viene aperta la porta dopo che la serratura avrà completato tutti i giri, questa tornerà
in posizione di chiusura, con scrocco rilasciato o con mandate in base al selettore D&N.
Se il jumper "No Release" viene rimosso, il rilascio dello scrocco viene ignorato (per serrature senza lo scrocco).
Se il jumper "Error yes" viene rimosso, la serratura si richiuderà nel caso in cui incontra un ostacolo durante l'apertura. Con questo
jumper inserito, il motore interrompe il movimento se incontra un ostacolo.


ENGLISH

This project controls a motorized door lock. It's a complete re-write of an italian commercial product that is now obsolete and 
unsupported. It controls a 12V gearmotor MICROMOTORS E192-2S.12.49 through a power driver L6201P and uses a reed switch to sense the
open/close door status, a pushbutton to engage the open/unlock and a switch to decide whether the door must be locked when it's closed.

See table above for Atmel ATMega328P pin connections.


