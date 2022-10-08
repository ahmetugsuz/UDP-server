#include "send_packet.h"
#include <signal.h>
#include <ctype.h>
#include <time.h> 

#define BUFSIZE 256
#define IP "127.0.0.1"

//Ps: Alt dette kommentarene er bare noe jeg liker å lage for meg selv, og tenkte at det ville vært lettere for sensor å se og forstå hva jeg tenker.
/*
Overordna beskrivelse/forståelse av teksten (første øyekast):

    1. Det første jeg la merke til at i teksten sto det at vi istedenfor hostname med IP addresser skulle finne fram endepunkt med brukernavn
    dvs. at vi i hostname skal bruke brukernavmnet?

    2. Istedenfor sendto() funskjonen skal vi nå bruke prekoden som er gitt: send_packet()

    3. Upush-serveren er slik at den venter evig og ikke lagrer meldinger på disk. Merk at denne serveren ikke er en select type, 
    hvorav vi kan skrive i server, men mer en ventende server, som ikke har noe annet funksjonalitet annet enn dirigering og kobling mellom klienter.

    4.


*/

/*
    UPush-server, steg for steg:
    1. Under kjøringa av programmet starter vi opp først server (typ: "./server")
    2. Server er i ventende tilstand, den får enten registreringsmeldinger eller oppslagsmeldinger.
    3. Utifra dette utfører server operasjoner, gir også svar tilbake til klienter om status av hvordan det har gått med meldinger.

*/

/*
    Lage en UPush-server:

*/





//oppbevare alle klientene vi registrerer i en struct
struct klient{
    char ip_addr[INET_ADDRSTRLEN];
    int PORT;
    int innaktiv;
    struct klient *neste;
    time_t timeack;
    time_t curr_time;
    char navn[];
}__attribute__((packed));

struct klient *k = NULL;
struct klient *tmp = NULL;
struct klient *forste = NULL;
struct klient *sokt_client = NULL;
struct klient *sist_oppdatert_client = NULL;
int free_verdi = 0;
int antall_registrerte = 0; //global variabel

void lenke(struct klient *klient){
    klient->neste = tmp;
    tmp = klient;
    forste = tmp;
}

struct klient* finnKlient(struct klient *klient, char *navn){
    while(klient != NULL){
        if(!strcmp(klient->navn, navn)) return klient;
        klient = klient->neste;
    }
    return NULL;
}

int finn_innaktivitet(struct klient *klient, char *navn){
    while(klient != NULL){
        if(!strcmp(klient->navn, navn)) return klient->innaktiv;
        klient = klient->neste;
    }
    return 2; //betyr bare error eller NULL verdi at vi ikke fant
}

void free_klienter(struct klient *klient){
    if(klient == NULL){
        exit(EXIT_SUCCESS);
    }
    struct klient *neste_klient = NULL;
    while(klient != NULL){
        printf("----- Klient %s frigjores -----\n", klient->navn); // dette er bare tillegg 
        sleep(0); // bare for liten kul demonstrasjon om rekkefolgen og hvem som blir frigjort først. 
        printf("\n");
        if(klient->neste != NULL){
            neste_klient = klient->neste;
        }else{
            neste_klient = NULL;
        }
        free(klient);
        klient = neste_klient;
    }
}

void check_res(int res, char* msg){
    if(res == -1){
        perror(msg);
        /*Rydde:*/
        //...
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

void signal_handler(int signum){
    if (signum == SIGINT){
        printf("\nCtrl+C registred, exiting Server...\n");
        //free klienter
        printf("%d Klienter frigjores...\n", antall_registrerte);
        printf("\n");
        if(antall_registrerte == 0){
            printf("Lista er tom\n");
            printf("Alle klienter er frigjort...\n");
        }
        else if(antall_registrerte == 1){
            printf("----- Klient %s frigjores -----\n", k->navn);
            free(k);
        }else{
            free_klienter(k);
        }
        exit(EXIT_SUCCESS);
    }
}




int main(int argc, char *argv[]){

    if(argc < 3){
        fprintf(stderr, " Usage: %s <port> <tapssannsynlighet> \n", argv[0]);
        return EXIT_SUCCESS; //vi tar ikke dette som feil, men mer oversikt, så er filosofisk riktig for meg.
    }

    int fd; //file descriptor
    int rc = 0; //read counter
    struct sockaddr_in my_addr, src_addr; //my_addr: server, src_addr: client
    struct sockaddr_in IP_addr;
    char buf[BUFSIZE] = { 0 };
    
    fd_set fds; 
    socklen_t addr_len; 
    struct timeval timeout;

    char *msg;
    float tapssansynlighet;

    tapssansynlighet = (atoi(argv[2])/100.0f);
    set_loss_probability(tapssansynlighet);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    check_res(fd, "socket");

    addr_len = sizeof(struct sockaddr_in);

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(atoi(argv[1]));
    my_addr.sin_addr.s_addr = INADDR_ANY;


    rc = bind(fd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr_in));
    check_res(rc, "Bind"); //Hvis andre prøver på samme addresse, dvs. connecte seg til server, vil dette gi feilmelding og avslutte.
    
    signal(SIGINT, signal_handler);

    while(1){
        /*PLAN: hvis jeg kunne hatt nedtelli9ng for hvert 10. sekund, og sjekket hver node om det er noen som har timer over 30 sekunder*/
        FD_ZERO(&fds);
        FD_SET(fd, &fds);


        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        
        rc = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
        check_res(rc, "select");

        
        if(rc == 0){ //et sjekk hvert 5. sekund for å se om e client ikke har vært aktiv siste 30 sekundene, ved å ikke svare på oppslagsforespørsel/send reg.
            struct klient *neste_client = NULL;
            struct klient *last_client = k;
            
            while(last_client != NULL){
                last_client->curr_time = time(NULL);
                if(last_client->curr_time - last_client->timeack >= 30){
                    last_client->innaktiv = 1; //indikerer at clienten er innaktiv, så kan den fjernes senere hvis den ikke skal oppdatere seg eller logge seg inn igjen

                }
                if(last_client->neste != NULL){
                    neste_client = last_client->neste;
                }else{
                    neste_client = NULL;
                }
                last_client = neste_client;
            }
        }

        buf[0] = 0;
        if(FD_ISSET(fd, &fds)){ //for å ikke ha blokkerende funksjon, for at telleren ikke blir blokkert på å vente på noe

        rc = recvfrom(fd, buf, BUFSIZE-1, 0,
        (struct sockaddr*)&src_addr, &addr_len); //src_addr er klientens addresse
        check_res(rc, "recvfrom");
        buf[rc] = 0;
        printf("Fra Client mottok vi: %s\n", buf);
        
        //finner ut hvilke kommandoer vi skal kjøre
        char sjekkBuf[BUFSIZE] = { 0 };
        strcpy(sjekkBuf, buf);
        char *linje = strtok(sjekkBuf, " ");
        int teller_ord = 0;
        char* command1 = "NULL";
        char* command2 = "NULL";
        int navn_i_tredje = 0; // Er en annen type måte for boolsk verdi, bare teller opp, hvis riktige statement stemmer med at navn skal ligge i plass [3] - (starter indeks fra 0)
        char *nick = "NULL";
        int stopp_vent;
        char ip_str[INET_ADDRSTRLEN] = { 0 };
        int gyldig_nick = 0; //dette er en variabel som jeg setter for å si om hvorvidt navn/nick er gyldig, hvis ASCII formatet i navn ikke er gyldig, så skal ikke navn lagres i lista.
        char *argument0 = NULL, *argument2 = NULL, *argument3 = NULL, *argument4 = NULL;
        //her er det flere sjekk for at navn skal være gyldig og holder variabler for ulike deler i kommandoen.
       while(linje != NULL){
            if(teller_ord == 0){
                command1 = linje; argument0 = linje;
                if(!strcmp(linje, "PKT")) navn_i_tredje++;

            }
            if(teller_ord == 1){
                stopp_vent = atoi(linje); 
            }
            if(teller_ord == 2){
                command2 = linje; argument2 = linje;
                if(!strcmp(linje, "REG")) navn_i_tredje++;
                if(!strcmp(linje, "LOOKUP")) navn_i_tredje++;
            }
            if(teller_ord == 3){
                argument3 = linje;
            }
            if(teller_ord == 4){
                argument4 = linje;
            }
            if(navn_i_tredje == 2 && teller_ord == 3){ //hvis det er navn i kommandoen
                nick = linje; 
                //sjekk for om navn er storre enn 20 bytes
                int navn_lengde = strlen(nick);
                if(navn_lengde > 20){ //her antar jeg at hvis navn er større enn 20 bytes, trenger vi ikke å kaste vekk alt, men heller beholde 20 første bytsa. Dermed lagres dette navnet.
                    char nymsg[BUFSIZE] = { 0 };
                    strcpy(nymsg, "Fra Server: Navn kan ikke gaa over 20 bytes, saa vi kutter ned nick til 20 bytes/bokstaver\n");
                    nick[20] = 0;
                    
                    rc = send_packet(fd, nymsg, strlen(nymsg), 0,
                    (struct sockaddr*)&src_addr, sizeof(struct sockaddr_in));
                    check_res(rc, "send");
                }

                
                //sjekk for om navn har gyldige ASCII verdier
                int i;
                for(i = 0; i < navn_lengde; i++){ //Hvi dette ikke stemmer, altså det ikke er gyldig ASCII verdier, så skal ikke navn lagres i lista 
                    if(!isascii(nick[i]) || isspace(nick[i])){
                        gyldig_nick++; //når vi øker dette sier vi ifra til senere at dette ikke er gydlig nick.
                        char nymsg[BUFSIZE] = { 0 };
                        sprintf(nymsg, "Fra Server: Ugyldig ASCII verdi %c\n", nick[i]); 

                        rc = send_packet(fd, nymsg, strlen(nymsg), 0,
                        (struct sockaddr*)&src_addr, sizeof(struct sockaddr_in));
                        check_res(rc, "send");
                        
                    }
                }
            
            }
            //Her lagres navn selv med space, med antagelse som beskrevet nedenfor
            if(teller_ord == 4 && navn_i_tredje == 2){ //antagelse om at vi ikke sender error eller stopper klienten, men heller tar nicken som passer, og kutter forste delen til å være nick. 
                char nymsg[BUFSIZE] = { 0 };
                sprintf(nymsg, "Fra Server: Navn kan ikke ha space i seg, vi tar forste delen %s\n", nick); 

                rc = send_packet(fd, nymsg, strlen(nymsg), 0,
                (struct sockaddr*)&src_addr, sizeof(struct sockaddr_in));
                check_res(rc, "send");
            }
            
            teller_ord++;
            linje = strtok(NULL, " ");
            
        }

        
        //Her skjer registreringa ved riktig kommandoer til serveren.
        if(!strcmp(command1, "PKT") && !strcmp(command2, "REG") && gyldig_nick == 0){
            /*----- Registrering: ------*/
            
            //setter inn det fra klienten inn i enn buffer
            //buffer i form: nummer og nickname
            char nymsg[100] = { 0 };

            

            struct klient *oppdater_client = NULL;

            if((oppdater_client = finnKlient(forste, nick)) == NULL){
                printf("Vi registrerer klient %s\n", nick);
                k = malloc(sizeof(struct klient) + strlen(nick) +1); //burde vært realloc -------------
                inet_pton(AF_INET, IP, &IP_addr.sin_addr); 
                k->PORT = htons(src_addr.sin_port);
                inet_ntop(AF_INET, &(IP_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                strcpy(k->ip_addr, ip_str);
                strcpy(k->navn, nick);
                k->timeack = time(NULL);
                k->curr_time = time(NULL);
                k->innaktiv = 0;
                lenke(k); //Lager en lenkeliste over registrerte brukere i server.
                antall_registrerte++;

                sprintf(nymsg, "ACK %d OK", stopp_vent);

                printf("Navn: %s\n", k->navn);
                printf("IP: %s\n", k->ip_addr);
                printf("PORT: %d\n", k->PORT);
            }else{  
                printf("Vi oppdaterer informasjon om klient %s\n", nick);
                oppdater_client->timeack = time(NULL);
                oppdater_client->curr_time = time(NULL);
                inet_pton(AF_INET, IP, &IP_addr.sin_addr); 
                oppdater_client->PORT = htons(src_addr.sin_port);
                inet_ntop(AF_INET, &(IP_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                strcpy(oppdater_client->ip_addr, ip_str);
                strcpy(oppdater_client->navn, nick);
                oppdater_client->innaktiv = 0;

                sprintf(nymsg, "Oppdatering: ACK %d OK", stopp_vent);

                printf("Navn: %s\n", oppdater_client->navn);
                printf("IP: %s\n", oppdater_client->ip_addr);
                printf("PORT: %d\n", oppdater_client->PORT);
            }

            
            printf("\n");

            rc = send_packet(fd, nymsg, strlen(nymsg), 0,
            (struct sockaddr*)&src_addr, sizeof(struct sockaddr_in));
            check_res(rc, "send");

            
        }
        else if(!strcmp(command1, "PKT") && !strcmp(command2, "LOOKUP")){    

           struct klient *tmp = k; 
           struct klient *finn_klient = k;

           struct klient *sokt_klient = finnKlient(tmp, nick);
           int innaktivitet = finn_innaktivitet(finn_klient, nick);

            if(sokt_klient != NULL && innaktivitet == 0){ //hvis vi finner brukeren i serveren over lagrede klienter sammen med at den brukeren er aktiv, så..->
                printf("Found the nick: %s\n", nick);
                char nymsg[BUFSIZE] = { 0 };
                sprintf(nymsg, "ACK %d NICK %s IP %s PORT %d \n", stopp_vent, sokt_klient->navn, sokt_klient->ip_addr, sokt_klient->PORT);
       
                rc = send_packet(fd, nymsg, BUFSIZE-1, 0,
                (struct sockaddr*)&src_addr, sizeof(struct sockaddr_in));
                check_res(rc, "send");

            }else{ //hvis ikke brukeren er i lista over lagrede klienter eller er innaktiv
                char nymsg[BUFSIZE] = { 0 };
                sprintf(nymsg, "ACK %d NOT FOUND \n", stopp_vent);
  
                rc = send_packet(fd, nymsg, BUFSIZE-1, 0,
                (struct sockaddr*)&src_addr, sizeof(struct sockaddr_in));
                check_res(rc, "send");
            }
             

        }
        else if(!strcmp(argument0, "ACK") && !strcmp(argument2, "NOT") && !strcmp(argument3, "FOUND")){
            struct klient *siste_client = k;
            struct klient *client_fjernes = finnKlient(siste_client, argument4);

            fprintf(stdout, "VI sletter client %s fra server pga innaktivitet\n", argument4);

            if(client_fjernes != NULL){
                struct klient *forrige = siste_client;                
                if(forrige != client_fjernes){
                    while(forrige != NULL && forrige->neste != client_fjernes){
                        forrige = forrige->neste;
                    }
                    if(client_fjernes->neste != NULL){
                        forrige->neste = client_fjernes->neste;
                    }
                }
                //egt så er k og forste samme, bare dårlig navn og duplikater for å holde styr
                if(client_fjernes == forste && forste->neste != NULL){ //om klienten som skal fjernes er den forste i lista, flytter vi til neste
                    forste = forste->neste;
                }
                if(k == client_fjernes && k->neste != NULL){
                    k = k->neste;
                }
                antall_registrerte--;             
                free(client_fjernes);
            }

            char nymsg[BUFSIZE] = { 0 };
            sprintf(nymsg, "ACK %d NOT FOUND \n", stopp_vent);
  
            rc = send_packet(fd, nymsg, BUFSIZE-1, 0,
            (struct sockaddr*)&src_addr, sizeof(struct sockaddr_in));
            check_res(rc, "send");            
            
        }
        else{
            msg = "WRONG FORMAT \n";
            rc = send_packet(fd, msg, strlen(msg), 0,
            (struct sockaddr*)&src_addr, sizeof(struct sockaddr_in));
            check_res(rc, "send");
        }
        }
        

    }
    
    
    close(fd);
    return EXIT_SUCCESS;
}


