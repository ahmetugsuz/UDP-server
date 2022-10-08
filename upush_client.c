#include "send_packet.h"
#include <time.h> 
#include <ctype.h>
#define BUFSIZE 4000 //vi må ha noe som passer og som er nok for alt, om oppgaven sier melding kan ikke vøre større enn 1400 bytes, vet vi at meldingen kan være lang, så vi må bare sikre oss med å få inn nok plass i bufferet. 
#define IP "127.0.0.1"
#define MELDING_STORRELSE 1400
#define PAKKE_STORRELSE 1500
/*PROBLEM: når vi går til rc == 0, så endres sekvensnummer (tror det er da vi slår opp i server, da sendes en annen sekvens, også får vi en annen sekven tilbake) 
s¨når vi sender mld på nytt, 3. gang ellerno sendes det med en annen sekvensnummer*/
struct data{
    char fra_navn[20];
    struct data *neste;
    int sekvensnummer;
    char* melding;
}__attribute__((packed));

struct client{
    char ip_addr[INET_ADDRSTRLEN];
    int PORT;
    struct client *neste;
    int sekvensNummer;
    char navn[20];
    int blokkert; //hvis 0 ikke blokkert, hvis 1 så er clienten blokkert.
}__attribute__((packed));

struct melding{
    int sekvensnummer;
    int fikk_svar;
    char *fra_nick;
    char *til_nick;
    struct melding *neste;
    char *melding;
}__attribute__((packed));

struct melding hold_melding;
struct melding hold_melding_tmp;

struct client *tmp = NULL;
struct client *c = NULL;
void lag_cache(struct client *client){
    client->neste = tmp;
    tmp = client;
}

struct data *tmp_mld = NULL;
struct data *data = NULL;
void cache_mld(struct data *d){
    d->neste = tmp_mld;
    tmp_mld = d;
}


void free_client(){

    struct data *midl = data;
    while(data != NULL){
        if(data->neste != NULL){
            midl = data->neste;
        }else{
            midl = NULL;
        }
        free(data->melding);
        free(data);
        data = midl;
    }


    struct client *neste_client = NULL;
    while(c != NULL){
        if(c->neste != NULL){
            neste_client = c->neste;
        }else{
            neste_client = NULL;
        }
        free(c);
        c = neste_client;
    }

    if(hold_melding.melding != NULL){
        free(hold_melding.melding);
    }
    
}

struct client* find_client(struct client *client, char* name){
    while(client != NULL){
        if(!strcmp(client->navn, name)){
            return client;
        }
        
        client = client->neste;
        
    }
    return NULL;
}


void check_res(int res, char* msg){
    if(res == -1){
        perror(msg);
        free_client();
        exit(EXIT_FAILURE);
    }
}

void get_string(char buf[], int size){
    char c;
    fgets(buf, size, stdin);

    if(buf[strlen(buf) -1] == '\n'){
        buf[strlen(buf)-1] = 0;
    }
    else
        while((c = getchar()) != '\n' && c != EOF);
}

//Alle de tre funksjonene nedenfor med send, kunne vært en funkjson, men kun for å holde det litt mer åpent på hva jeg gjør, sånn at det er leselig, så bruker jeg den strukturen, men ellers kunne vi hatt send_to_x(...)
void send_to_server(int fd, char melding[], int storrelse, struct sockaddr_in server_addr){
    int rc = send_packet(fd, melding, storrelse, 0
    , (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
    check_res(rc, "send_packet");
}

void send_to_friend(int fd, char melding[], int storrelse, struct sockaddr_in friend_addr){
    int rc = send_packet(fd, melding, storrelse, 0
    , (struct sockaddr*)&friend_addr, sizeof(struct sockaddr_in));
    check_res(rc, "sendto");

}

void send_back_to_from(int fd, char melding[], int storrelse, struct sockaddr_in from_addr){
    int rc = send_packet(fd, melding, storrelse, 0
    , (struct sockaddr*)&from_addr, sizeof(struct sockaddr_in));
    check_res(rc, "sendto");
}




int main(int argc, char *argv[]){

    if(argc < 6){
        fprintf(stderr, " Usage: %s <NICK> <ADRESSE> <PORT> <TIMEOUT> <TAPSSANNSYNLYGHET> \n", argv[0]);
        return EXIT_SUCCESS;
    }
    /*------ SOCK_DGRAM: UPD (connectionless)-connection -------*/
    int fd; 
    int rc, wc;
    fd_set fds; 
    struct sockaddr_in server_addr, friend_addr, from_addr;
    socklen_t addr_len;
    char buf[BUFSIZE];
    //int antall_ord = 0;
    struct timeval timeout;

    int lookup_gyldig = 0;

    char *nick = argv[1];
    char *server_IP = argv[2];
    int server_PORT = atoi(argv[3]);
    int tidsbegrensning = (double)atoi(argv[4]);
    float tapssansynlyghet = ((atoi(argv[5]))/100.0f);
    set_loss_probability(tapssansynlyghet);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    check_res(fd, "socket");

    addr_len = sizeof(struct sockaddr_in);
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_PORT);
    wc = inet_pton(AF_INET, server_IP, &server_addr.sin_addr.s_addr);
    check_res(wc, "inet_pton");
    if(!wc){
        fprintf(stderr, "Invalid IP adress %s\n", server_IP);
        return EXIT_FAILURE;
    }
    /*------/Connection-------*/    
    printf("Skriv inn en kommandolinje (q | quit | exit for avslutt klient)\n");

    /*-------Skrive/Lese------*/

    int sekvensNummer = 0; 
    char nybruker[BUFSIZE] = { 0 };
    sprintf(nybruker, "PKT %d REG %s", sekvensNummer, nick);
    
    rc = send_packet(fd, nybruker, BUFSIZE-1, sekvensNummer
    , (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
    check_res(rc, "sendto");
    //hvis den har gått gjennom send_packet

    
    //setter timer, dette er for send_packet på første hvor vi REG nick med engang
    timeout.tv_sec = tidsbegrensning;
    timeout.tv_usec = 0;
    char* fra_nick = nick;
    char* til_nick = NULL;
    char* command1 = NULL, *command2 = NULL, *command3 = NULL, *command4 = NULL;
           
    int fikk_svar = 1;
    int svar_melding = 0;
    int melding_sendt = 0;
    int forsok = 0, forste_reg = 1, venter_svar = 0;
    int tilbake_melding = 0, pakke_til_server = 0, pakke_til_klient = 0, forsok_til_klient = 0, to_ganger_til = 0, mottat_melding = 0;
    int innaktive_forsok = 0;
    time_t timeack, curr_time;
    
    while(1){
        
        timeack = time(NULL);

        FD_ZERO(&fds);

        FD_SET(fd, &fds);
        FD_SET(STDIN_FILENO, &fds);

        rc = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
        check_res(rc, "select");
        
        if(pakke_til_server > 0 || pakke_til_klient > 0 || forste_reg > 0 || venter_svar > 0){
            timeout.tv_sec = tidsbegrensning;
        }else{
            timeout.tv_sec = 30; //vi vil ikke at dette skal skje, dermed setter vi bare høyt tall, vi handler den situasjonen nedenfor.
        }

        if(((sekvensNummer + 1) % 2) == 0){
            sekvensNummer--;
        }
        else if(((sekvensNummer + 1) % 2) == 1){
            sekvensNummer++;
        }
        
        curr_time = time(NULL); // tar current time
            /*den her vil kun være true når tidsavbrudd skjer og summen av cuurent - timeack 
            (som vi starter hver gang while loopen kjøres om igjen) og da den er 10 sekunder og vil starte om igjen til å telle til 10 sek.*/
            if(curr_time - timeack >= 10){ 
                //printf("Timer = %ld\n", curr_time - timeack);
                innaktive_forsok++;

                char oppdater[BUFSIZE] = { 0 };
                sprintf(oppdater, "PKT %d REG %s", sekvensNummer, nick);
                venter_svar++;

                rc = send_packet(fd, oppdater, BUFSIZE-1, 0
                , (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
                check_res(rc, "sendto");

                
            }
            
        
        /*Tilfeller*/
        //1.Hvis vi i starten sender REG pakke og tidsavbruddet kommer
        //2.Når pakke sendes og serveren eller klienten svarer etter tidsavbruddet
        // 3. Når vi sender en oppslag til server, og vi ikke mottar svar fra serveren innen tidsavbruddsperioden
        if (rc == 0){ 
            //--------------------------DET ER FEIL I RC DA VI SKAL HA RANDOM PACKET SENDING, FORDI VI FAILER MED SEKVENSNUMMER GÅR DET TIL DUPLIKAT 
            melding_sendt++;
            /*Tilfeller:*/
            /*1 Vår første melding: Vi skal sende vår første melding, vi går over til en oppslagsprossedyre, her vil vi prøve totalt 3 ganger. Om forsøk når 3 ganger så avslutter vi ellers fortsetter vi->*/
            /*2: Sende meldingen til clienten: Nå har vi fått oppslag fra server og er klar for sending av melding. Hvis vår melding går ut av timeout, har vi et tilfelle hvor vi prøver å sende på nytt. */
            /*3: Går meldingen? Hvis meldingen mottas, må mottager sende ACK tilbake, denne vil også prøve seg med oppslagsprossedyre og prøve på nytt til ACK er motatt, ellers gi opp. Men hvis vår melding ikke går som avsender, vil vi prøve på oppslag på nytt*/
            /*4: Finner vi noe nytt? Hvis server gir oss noe nytt informasjon, prøver vi å sende på nytt 2 ganger til, men ellers gir vi opp med å sende meldingen*/
            if(forste_reg > 0 || venter_svar > 0){
                fprintf(stderr, "Vi mottok ikke svar innen tidsavbrudd, vi avslutter client\n");
                /*free*/
                break;
            }

            if(pakke_til_klient > 0 && forsok == 1){
                //printf("OI, client faila igjen, vi tar en ny oppslagsprossedyre");
                pakke_til_klient = 0; 
                forsok_til_klient++; 
                pakke_til_server++; fikk_svar = 1;
                forsok = -1;//Fordi vi skal prøve på opplagsprossedyren 3 ganger til totalt som beskrevet i oppgaveteksten
                to_ganger_til++; //den her vil øke dersom vi sender noe melding etter oppslag, altså er dette en variabel som øker til senere for sending av melding hvor vi skal tilslutt prøve på nytt 2 ganger til etter server som har gitt oppslag.
                //printf("Forsok til client = %d\n", forsok_til_klient);

            }
            if(pakke_til_klient > 0 && forsok < 1){  //tilfelle da client skal sende paa nytt 
                //printf("Vi prover aa sende pakke til client paa nytt\n");
                forsok++; 
                forsok_til_klient++; to_ganger_til = 0;
                char melding_pakke[1500] = { 0 }; //lager ekstra plass for andre kommandoer, navn  osv..
                sprintf(melding_pakke, "PKT %d FROM %s TO %s MSG %s", c->sekvensNummer, nick, c->navn, hold_melding.melding);
                hold_melding.sekvensnummer = c->sekvensNummer;
                send_to_friend(fd, melding_pakke, PAKKE_STORRELSE-1, friend_addr); //PAKKE_STORRELSE -1 fordi vi ikke tar med 0 byte
            }
            else if(pakke_til_server > 0 && fikk_svar != 0 && forsok < 2){//tilfelle for da server skal sende paa nytt
                //printf("Oppslagsprosedyre, forsok = %d\n", forsok);
                forsok++;
                char sjekk_server[BUFSIZE] = { 0 };
                sprintf(sjekk_server, "PKT %d LOOKUP %s", hold_melding.sekvensnummer, hold_melding.til_nick);
                send_to_server(fd, sjekk_server, BUFSIZE-1, server_addr);
                
                
            }
            else if(forsok_til_klient > 1 && forsok == 2){ //tilfelle vi ikke lykkes med oppslag //alt maa nullstilles
                fprintf(stderr, "NICK %s NOT REGISTRED\n", hold_melding.til_nick);
                timeout.tv_sec = 10;
                pakke_til_server = 0; forsok_til_klient = 0;
                pakke_til_klient = 0; fikk_svar = 0; to_ganger_til = 0;
                forsok = 0;
            }else if(to_ganger_til > 1 && to_ganger_til < 3){ //tilfelle vi lykkes med oppslag og skal sende pakke to ganger til.
                //printf("EN GANG TIL!\n");
                to_ganger_til++; forsok_til_klient = 0;
                timeout.tv_sec = tidsbegrensning;
                char melding_pakke[1500] = { 0 }; //lager ekstra plass for andre kommandoer, navn  osv..
                sprintf(melding_pakke, "PKT %d FROM %s TO %s MSG %s", c->sekvensNummer, nick, c->navn, hold_melding.melding);
                send_to_friend(fd, melding_pakke, PAKKE_STORRELSE-1, friend_addr); //PAKKE_STORRELSE -1 fordi vi ikke tar med 0 byte
            }
            else if(to_ganger_til == 3){
                fprintf(stderr, "NICK %s UNREACHABLE\n", hold_melding.til_nick);
                timeout.tv_sec = 10;
                pakke_til_server = 0; forsok_til_klient = 0;
                pakke_til_klient = 0; fikk_svar = 0; to_ganger_til = 0;
                forsok = 0;
            }

                        
            
        }
        
        forste_reg = 0;
        venter_svar = 0;
        //input fra socket, melding som blir lest, som blir sendt av andre, altså en mottager. Kan evt. være server eller en annen upush_client.
        if(FD_ISSET(fd, &fds)){//hvis det kommer mld fra andre side/parten (fra mottager)
            //hvis det kommer fra mottager skal vi lese forst

            rc = read(fd, buf, BUFSIZE-1);
            check_res(rc, "read");
            buf[rc] = 0;
            timeout.tv_sec = 10;
            timeack = time(NULL);
            char msg[MELDING_STORRELSE +1] = { 0 }; //initialiserer med 0 byte. "+1" forklart senere i koden
            char sjekkbuf[BUFSIZE];
            strcpy(sjekkbuf, buf);
            char *linje = strtok(sjekkbuf, " ");
            int antall_ord = 0;
            int stop_wait = 0;
            char* argument0 = NULL, *argument1 = NULL, *argument2 = NULL, *argument3 = NULL, *argument4 = NULL, 
            *argument5 = NULL, *argument6 = NULL, *argument7 = NULL;
            int det_er_tekst = 0;
            while(linje != NULL){
                if(antall_ord == 0){
                    argument0 = linje;
                    command1 = linje;
                }
                if(antall_ord == 1){
                    argument1 = linje; stop_wait = atoi(linje);
                }
                if(antall_ord == 2){
                    argument2 = linje;
                    command2 = linje;
                }
                if(antall_ord == 3){
                    argument3 = linje;
                    fra_nick = linje;
                }
                if(antall_ord == 4){
                    argument4 = linje;
                    command3 = linje;
                }
                if(antall_ord == 5){
                    argument5 = linje;
                    til_nick = linje;
                }
                if(antall_ord == 6){
                    argument6 = linje;
                    command4 = linje;
                    if(!strcmp(argument6, "MSG")){
                        det_er_tekst++;
                    }
                }
                if(antall_ord == 7){
                    argument7 = linje;
                }
                if(antall_ord >= 7 && det_er_tekst > 0){
                    strcat(msg, linje);
                    strcat(msg, " ");
                }

                antall_ord++;
                linje = strtok(NULL, " ");
            }


            /*tilfeller:*/
            //ACK nummer OK : SERVER/CLIENT
            //PKT nummer FROM fra_nick TO til_nick MSG tekst : NICK->Client
            //ACK nummer NICK nick IP adresse PORT port : SERVER
            

            if(!strcmp(command1, "ACK") && !strcmp(command2, "OK")){
                if(hold_melding.melding != NULL){ //dette er noe med siden vi sender regiustreringsmeldinger hvert 10. sekund og vi får en ACK tilbake av server, så må ikke det free vår melding
                    free(hold_melding.melding);
                    hold_melding.melding = NULL;
                }
                fprintf(stderr, "%s\n", buf); 
                forsok = 0; //vi har motatt pakken, forsøk settes tilbake til 0. Nullstille alt her.
                melding_sendt = 0;      
            }

            //@nick: MSG tilfelle
            //case: PKT 0 LOOKUP til_nick // dette er bare for å lagre i cache til å bruke senere, det er ikke sikkert at det blir sendt melding til samme nick med enagng.
                                    /*--------------Fra SERVER----------------*/
            if(!strcmp(argument0, "ACK") && !strcmp(argument2, "NICK") && !strcmp(argument4, "IP") && !strcmp(argument6, "PORT")){
                //hvis oppslag er vellykket, så sender ikke server svar: ACK 0 OK, men informasjon om clienten, da kan vi bruke fortsatt variabelen forsok
                
                    
                    if(svar_melding > 0){ //client skal sende en melding
                        from_addr.sin_family = AF_INET;
                        from_addr.sin_port = htons(atoi(argument7));
                        rc = inet_pton(AF_INET, argument5, &from_addr.sin_addr.s_addr);
                        check_res(rc, "inet_pton");
                        if(!rc){
                            fprintf(stderr, "inavlid IP adress %s\n", argument5);
                        }
                        
                        char tilabkemelding[BUFSIZE] = { 0 };
                        sprintf(tilabkemelding, "ACK %d OK", stop_wait);
                        mottat_melding++;
                        send_back_to_from(fd, tilabkemelding, BUFSIZE-1, from_addr);

                        svar_melding--;
                        
                    }
                    else{ //tilfelle: Vi skal lagre info til client i struct client
                    
                        printf("Fra server: %s\n", buf);
                        lookup_gyldig++; // noe som teller til client om at vi har funnet ved oppslag, så du kan sende pakke til clienten
                        c = malloc(sizeof(struct client));
                        strcpy(c->navn, argument3); 
                        strcpy(c->ip_addr, argument5);
                        c->PORT = atoi(argument7); 
                        c->sekvensNummer = stop_wait;
                        c->blokkert = 0;
                        lag_cache(c);

                        friend_addr.sin_family = AF_INET;
                        friend_addr.sin_port = htons(atoi(argument7));
                        rc = inet_pton(AF_INET, c->ip_addr, &friend_addr.sin_addr.s_addr);
                        check_res(rc, "inet_pton");
                        if(!rc){
                            fprintf(stderr, "inavlid IP adress %s\n", c->ip_addr);
                        }
                        
                        
                        char melding_pakke[1500] = { 0 }; //lager ekstra plass for andre kommandoer, navn  osv..
                        sprintf(melding_pakke, "PKT %d FROM %s TO %s MSG %s", c->sekvensNummer, nick, c->navn, hold_melding.melding);
                        melding_sendt++;
                        pakke_til_klient++;
                        timeout.tv_sec = tidsbegrensning;
                        
                        
                        if(to_ganger_til > 0){
                            to_ganger_til++; 
                            fikk_svar = 0; pakke_til_server = 0; forsok = 0; //nullstiller alt 
                            pakke_til_klient = 0; //Fordi her har vi motatt fra server i oppslag, da skal vi ikke skrive ut NICK NOT REGISTRED
                        }else{
                            fikk_svar = 0; forsok = 0; pakke_til_klient++; to_ganger_til = 0;
                        }
                        
                        
                        send_to_friend(fd, melding_pakke, PAKKE_STORRELSE-1, friend_addr); //PAKKE_STORRELSE -1 fordi vi ikke tar med 0 byte

                }
                
                

            }
            else if(!strcmp(argument0, "ACK") && !strcmp(argument2, "NOT") && !strcmp(argument3, "FOUND")){ 
                fprintf(stderr, "NICK %s NOT REGISTRED\n", hold_melding.til_nick); 
                free(hold_melding.melding);
                forsok = 0; melding_sendt = 0;
                pakke_til_server = 0; forsok_til_klient = 0;
                pakke_til_klient = 0; fikk_svar = 0; to_ganger_til = 0;
                fikk_svar = 0; //bare et tall som sier vi har ikke klart å finne navn, kommer fra server
            }
            else if(!strcmp(argument0, "Fra") && !strcmp(argument1, "Server:")){
                fprintf(stdout, "%s\n", buf);
            }else if(!strcmp(argument0, "Oppdatering:")){
                //trenger ikke å skrive noe her da det skjer en oppdatering av informasjon i server. vi har dette pga. tidsavbruddet vi har satt til å motta noe fra server.
            }
            
            
                                    /*--------------Fra CLIENT----------------*/
            if (!strcmp(command1, "PKT") && !strcmp(command2, "FROM") && !strcmp(command3, "TO") && !strcmp(command4, "MSG")){

                pakke_til_klient = 0;
                pakke_til_server = 0;
                timeout.tv_sec = 10;
                forsok = 0;
                melding_sendt = 0;

                struct client *client_sjekk = find_client(c, fra_nick);

                if(client_sjekk != NULL && client_sjekk->blokkert == 1){
                    //blokkert
                }else{

                    /*Lager kø av meldinger*/
                    data = malloc(sizeof(struct data));
                    data->melding = strdup(msg);
                    strcpy(data->fra_navn, fra_nick);
                    data->sekvensnummer = stop_wait;
                    
                    cache_mld(data); 
                    /*Etter å ha lagre sekvsnummeret, kan vi nå endre på det for neste pakke som sendes*/
                    
                    fra_nick = argument3; 
                    til_nick = argument5;

                    /*--------Sjekker nick og format og printer ut meldinger--------*/

                 //sjekker om til_nick stemmer, grunnen til at vi gjør det først er for å eliminere de andre meldingene som kommer fra andre klienter,
                    //når vi skal sjekke om det potensielt er duplikater, sjekker vi om vi har fått samme sekvensnummer. Siden hver klient har sin kø og antall av meldinger, vil sekvensnummeret endre seg for hver
                    //melding som kommer. Dvs. at kun hvis samme melding sendes, vil vi ikke ha unikt sekvensnummer, altså den vil ikke endre seg, ved duplliakt ville det sett slikt ut: 1 0 0 1 0 1 0 ... 
                    //siden 0 forekommer 2 ganger etter hverandre vet vi at samme melding har blitt sendt på nytt, potensielt av Ack ikke har kommet til avsender.
                    
                   
                    
                        if(!strcmp(nick, til_nick)){
                            if(data->neste != NULL && data->neste->sekvensnummer == data->sekvensnummer && !strcmp(data->fra_navn, data->neste->fra_navn)){ //her sier vi først sjekk om neste peker ikke er null for å ikke få seg.faul. også sjekker vi om forrige sekvens var lik den som kom nå
                                //, i det tilfelle så sjekker vi også om det kom fra samme person. Altså client, antagelsen her er at to ulike clienter kan samme samme sekvensnummer, men samme client kan ikke sende samme sekvensnummer to ganger til en client, da er det duplikat.
                                fprintf(stdout, "Duplikat\n");
                            }else{
                                fprintf(stdout, "%s: %s \n", fra_nick, data->melding);  
                            }
                            tilbake_melding = 1;   //svarer som vanlig med ACk
                        }
                        else if(strcmp(nick, til_nick)){
                            fprintf(stdout, "Noen provde aa sende melding med Feil nick\n");
                            tilbake_melding = 2;
                        }else{
                            fprintf(stdout, "Noen provde aa sende melding med Wrong format\n");
                            tilbake_melding = 3;
                        }
                        
                    
                    
                    /*--------Sjekker nick og format--------*/
                    //Sende svar tilbake 'ACK nummer OK'
                    //poenget her er at vi skal nå sende melding tilbake, dvs. at vi må tenke oss nå som mottager, og tenke at vi sender melding tilbake til "oss". 

                    /*--------Sender svar tilbake--------*/
                    
                    struct client *tmp2 = c;
                    struct client *fra_c = find_client(tmp2, fra_nick);
                    if(fra_c != NULL){ //leter først på cache
                        from_addr.sin_family = AF_INET;
                        from_addr.sin_port = htons(fra_c->PORT);
                        rc = inet_pton(AF_INET, fra_c->ip_addr, &from_addr.sin_addr.s_addr);
                        check_res(rc, "inet_pton");
                        if(!rc){
                            fprintf(stderr, "inavlid IP adress %s\n", fra_c->ip_addr);
                        }

                        mottat_melding++;
                        if(tilbake_melding == 1){
                            char tilbakemelding[BUFSIZE] = { 0 };
                            sprintf(tilbakemelding, "ACK %d OK", data->sekvensnummer);
                            send_back_to_from(fd, tilbakemelding, BUFSIZE-1, from_addr);
                        }
                        else if(tilbake_melding == 2){
                            char tilbakemelding[BUFSIZE] = { 0 };
                            sprintf(tilbakemelding, "ACK %d WRONG NAME", data->sekvensnummer);
                            send_back_to_from(fd, tilbakemelding, BUFSIZE-1, from_addr);
                        }
                        else if(tilbake_melding == 3){
                            char tilbakemelding[BUFSIZE] = { 0 };
                            sprintf(tilbakemelding, "ACK %d WRONG FORMAT", data->sekvensnummer);
                            send_back_to_from(fd, tilbakemelding, BUFSIZE-1, from_addr);
                        }
                    }else{                        
                        char sok_bruker[BUFSIZE] = { 0 };
                        sprintf(sok_bruker, "PKT %d LOOKUP %s", data->sekvensnummer, fra_nick);
                        svar_melding = 1;
                        send_to_server(fd, sok_bruker, BUFSIZE-1, server_addr); //sender til server i en funkjson, gjør det lettere å holde oversikt og kortere 
                        
                    }
                }
            }

            
            if(!strcmp(argument0, "ACK") && !strcmp(argument2, "WRONG")){
                melding_sendt = 0;
                fprintf(stderr, "%s\n", buf);
            }

            if(!strcmp(argument0, "WRONG") && !strcmp(argument1, "FORMAT")){
                fprintf(stdout, "%s\n", buf);
            }
                
            
            
        }
        

       if(FD_ISSET(STDIN_FILENO, &fds)){//hvis det er vi som skriver, altså som en client
            //Da skal vi sende til mottager

            //setter timer
            //dette er for å sette timeren til den tidsbegrensingen som er oppgitt i argumenter. SÅ tiden som klinten gir under pakke sending.
            timeout.tv_sec = tidsbegrensning;            
            
            curr_time = time(NULL);
            get_string(buf, MELDING_STORRELSE); //henter string med fgets.. setter inn i buffer
            
            //Avslutte client
            if(!strcmp(buf, "q") || !strcmp(buf, "quit") || !strcmp(buf, "exit") || !strcmp(buf, "QUIT")){
                printf("Client avsluttet \n");
                free_client();
                break;
            }   
            

            //tilfeller:
            //1. PKT 0 REG nick
            //2. PKT 0 LOOKUP til_nick 
            
            char *argument0 = NULL, *argument1 = NULL, *argument2 = NULL, *argument3 = NULL;
            char sjekkbuf[MELDING_STORRELSE], melding[MELDING_STORRELSE + 1] = { 0 }; //Plass til 1400 bokstaver (og mellomrom) pluss null-byte
            char sjekkMelding[BUFSIZE] = { 0 }; //laget for å kutte til 1400 bytes/ Unngår seg faul
            strcpy(sjekkbuf, buf);
            char *linje = strtok(sjekkbuf, " ");
            int antallord = 0;
            char til_nick[20 + 1] = { 0 }; //20 bokstaver med navn og + 1 for 0-byte
            char *t_nick = NULL;
            int navn_strl = 0;
            int meldingStrl = 0, ugyldigAscii = 0;
            innaktive_forsok = 0;
            if(linje[0] == '@'){ //hvis første bokstav er @ da veit vi at vi sender melding.
                t_nick = linje;
                navn_strl = (int)(strlen(t_nick));
                if(navn_strl > 20){ //for å ikke få seg faul eller error sjekker vi bare for den sikre siden at brukere ikke taster lange navn.
                    fprintf(stdout, "Ingen navn kan vore storre enn 20 bytes\n");
                }else{
                    int k = 0;
                    for(int i = 1; i <= navn_strl; i++){
                        til_nick[k] = t_nick[i];
                        k++; 
                    }
                }
                //til_nick er til det navnet vi skal sende til
                linje = strtok(NULL, " "); //fortsetter her til melding
                while(linje != NULL && meldingStrl <= 1400){ //sånn at vi ikke looper gjennom en veldig lang melding om d skulle være tilfelle
                    meldingStrl += strlen(linje);
                    strcat(sjekkMelding, " "); //mellomrom for mellom ordene
                    strcat(sjekkMelding, linje);
                    for(u_int8_t j = 0; j < strlen(linje); j++){
                        if(!isascii(linje[j])){
                            ugyldigAscii++;
                        }
                    }

                    linje = strtok(NULL, " ");
                }
                /*kontrollerer melding og format*/
                sjekkMelding[1400] = 0; // hvis det er lengre eller kortere bare spesifiserer vi at vi har ikke noe etter 1400 bytes, så setter av en slags "punktum".
                if(ugyldigAscii > 0){
                    timeout.tv_sec = 10; 
                    fprintf(stderr, "Ugyldig ascii verdi\n");
                }
                
                strcpy(melding, sjekkMelding); //nå kan vi ta meldingen vår, etter å ha sjekke antall bytes.

                /*Holde melding om vi skal først søke gjennom server*/
                
                
                
                hold_melding.fra_nick = fra_nick;
                hold_melding.til_nick = til_nick;
                hold_melding.sekvensnummer = sekvensNummer;
                hold_melding.melding = strdup(melding);
                hold_melding.fikk_svar = 1;
                

                struct client *til_client = find_client(c, til_nick);
                
                    
                
                if(til_client != NULL && ugyldigAscii == 0){ //Da vet vi at vi har den i cache
                    
                    friend_addr.sin_port = htons(til_client->PORT);
                    friend_addr.sin_family = AF_INET;
                    rc = inet_pton(AF_INET, c->ip_addr, &friend_addr.sin_addr.s_addr);
                    check_res(rc, "inet_pton");
                    if(!rc){
                        fprintf(stderr, "Invalid IP adress %s\n", c->ip_addr);
                    }
                    if(til_client->blokkert == 1){
                        fprintf(stdout, "Denne brukeren er blokkert\n");
                    }else{
                        if(((til_client->sekvensNummer + 1) % 2) == 0){
                            til_client->sekvensNummer--;
                        }
                        else if(((til_client->sekvensNummer + 1) % 2) == 1){
                            til_client->sekvensNummer++;
                        }
                        melding_sendt++;
                        
                        char melding_pakke[PAKKE_STORRELSE] = { 0 }; //lager ekstra plass for andre kommandoer, navn  osv..
                        sprintf(melding_pakke, "PKT %d FROM %s TO %s MSG", til_client->sekvensNummer, nick, til_client->navn);
                        strcat(melding_pakke, melding);
                        pakke_til_klient++; forsok = 0; //Her vil client ligge i cache så vi ikke trenger å ta oppslag i starten.
                        timeout.tv_sec = tidsbegrensning;
                        send_to_friend(fd, melding_pakke, PAKKE_STORRELSE-1, friend_addr);
                    }

                }
                else if(til_client == NULL && ugyldigAscii == 0){ //da vet vi at den ikke ligger i cache, så vi kjører på med oppslagsprossedyre
                    
                    char sjekk_server[BUFSIZE] = { 0 };
                    sprintf(sjekk_server, "PKT %d LOOKUP %s", sekvensNummer, til_nick);
                    pakke_til_server++; //sender videre til rc om det skulle være tilfelle select time out
                    send_to_server(fd, sjekk_server, BUFSIZE-1, server_addr);

                }
            


            }
            else{ //hvis ikke det er en melding vi skal sende
                /*Tilfeller:*/
                //PKT nummer REG nick
                //PKT nummer LOOKUP nick
                while(linje != NULL){
                    if(antallord == 0) argument0 = linje;
                    if(antallord == 1) argument1 = linje;
                    if(antallord == 2) argument2 = linje;
                    if(antallord == 3) argument3 = linje;
                    antallord++;
                    linje = strtok(NULL, " ");
                }
                if(!strcmp(argument0, "PKT") && !strcmp(argument2, "LOOKUP")){
                    char sok_bruker[BUFSIZE] = { 0 };
                    sprintf(sok_bruker, "PKT %d LOOKUP %s", sekvensNummer, argument3);
                    send_to_server(fd, sok_bruker, BUFSIZE-1, server_addr);
                }
                else if(!strcmp(argument0, "BLOCK")){
                    timeout.tv_sec = 30; //dette er fordi vi ikke skal motta noe
                    struct client *blokker_client = find_client(c, argument1);
                    if(blokker_client != NULL){
                        blokker_client->blokkert = 1;
                    }else{
                        fprintf(stdout, "Kunne ikke finne bruker %s\n", argument1);
                    }
                }
                else if(!strcmp(argument0, "UNBLOCK")){
                    timeout.tv_sec = 30; 
                    struct client *unblokker_client = find_client(c, argument1);
                    if(unblokker_client != NULL){
                        unblokker_client->blokkert = 0;
                    }else{
                        fprintf(stdout, "Kunne ikke finne bruker %s\n", argument1);
                    }

                }
                else if(!strcmp(argument0, "PKT") && !strcmp(argument2, "REG")){
                    char sok_bruker[BUFSIZE] = { 0 };
                    sprintf(sok_bruker, "PKT %d REG %s", sekvensNummer, argument3);
                    send_to_server(fd, sok_bruker, BUFSIZE-1, server_addr);
                }
                else{
                    fprintf(stderr, "WRONG FORMAT\n");
                }

            }


            //Neste stegene handler stort sett om å sende meldingen til "til_nick"
            /*-----------Sende melding----------*/

            //først og fremst vil vi sjekke om 


        }
        
    }
        
    
    close(fd);
    return EXIT_SUCCESS;
}

