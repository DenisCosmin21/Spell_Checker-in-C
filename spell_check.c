#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

char sizes[] = {42, 46, 48, 52, 64, 80, 128}; //multimea de dimensiuni pentru payload. Momentan vom limita si implemenatrea la 128 bytes pentru o simpla simulare

char **DDR3; //Cuvintele corecte vor fi salvate in ddr3 in ordine alfabetica pentru a eficientiza algoritmul
char ***DDR3_2; //Cuvintele inversate vor fi salvate in aceasta variabila pentru a le schimba.

typedef struct
{
  unsigned char preamble[7];
  unsigned char destination_mac[6]; //folosim unsigned char deoarece in caz contrar nu vom avea 8 biti de date in aceasta variabila
  unsigned char source_mac[6];
  unsigned char type;
  unsigned char payload[128];//am putea sa ii oferim de la inceut maximul de spatii in cazul in care dorim sa dam typecast in char
}PACKET_T; //Pentru o explicatie mai buna a algoritmului de impachetare vom folosi aceasta structura, dar va exista si o functie care va returna un sir de bytes exact cum ar functiona in verilog
//Dar pentru implementarea serverului cea mai buna varianta este aceasta structura, pentru o structurare mai buna a codului. Oricum si o structura este o insiruire de bytes in memorie

typedef enum{PREAMBLE_CHECK, PAYLOAD_READ}UNPACK_STATE;//Este folosit la despachetarea informatiei in automatul de stare

typedef enum{check_word, find_word_match, change_word, word_found, weight_compare}WORD_STATE;

void initial_begin(void)//Aceasta functie va aseza la inceput de program tot ce e nevoie in memoria DDR3, exact ca in verilog in modulul principal
{
  char *words[] = {"love", "happy", "fericit", "fericire", "succes"}; //lista cu cuvintele de asezat in memorie
  char *words_neg[] = {"hate", "sad", "trist", "tristete", "esec"};//lista cuvintelor negate
  char *aux;
  DDR3 = malloc(5 * sizeof(*DDR3));
  
  for(ssize_t i = 0;i < 5;i++){
    DDR3[i] = malloc(20 * sizeof(char));
    strcpy(DDR3[i], words[i]);
  }
  
  for(ssize_t i = 0;i < 4;i++){//Algoritmul sorteaza in ordine crescatoare cuvintele, dar in verilog le vom aseza noi in ordine
    for(ssize_t j = i + 1;j < 5;j++){
      if(strcasecmp(DDR3[i], DDR3[j]) > 0){
	aux = DDR3[i];
	DDR3[i] = DDR3[j];
	DDR3[j] = aux;
      }
    }
  }

  DDR3_2 = malloc(2 * sizeof(*DDR3_2)); //Vom avea un tablou de matrici de stringuri, pentru a putea face un fel de lok_up table
  DDR3_2[0] = malloc(5 * sizeof(**DDR3_2));
  DDR3_2[1] = malloc(5 * sizeof(**DDR3_2));
  
  for(ssize_t i = 0;i < 5;i++){
    DDR3_2[0][i] = malloc(20 * sizeof(char));
    DDR3_2[1][i] = malloc(20 * sizeof(char));
    strcpy(DDR3_2[0][i], words[i]);
    strcpy(DDR3_2[1][i], words_neg[i]);
  }

  char *aux2 = NULL;
  
  for(ssize_t i = 0;i < 4;i++){//Algoritmul sorteaza in ordine crescatoare cuvintele, dar in verilog le vom aseza noi in ordine
    for(ssize_t j = i + 1;j < 5;j++){
      if(strcasecmp(DDR3_2[0][i], DDR3_2[0][j]) > 0){
	aux = DDR3_2[0][i];
	aux2 = DDR3_2[1][i];
	DDR3_2[0][i] = DDR3_2[0][j];
	DDR3_2[0][j] = aux;
	DDR3_2[1][i] = DDR3_2[1][j];
	DDR3_2[1][j] = aux2;
      }
    }
  }
  
}

void end_program(void)
{
  for(ssize_t i = 0;i < 5;i++){
    free(DDR3[i]);
  }
  free(DDR3);
}

char *read_data(void)
{
  char *buffer = malloc(sizeof(char));
  ssize_t size = 0;
  ssize_t max_size = 1;
  char ch;
  while((ch = getchar()) && (ch != '\n')){//caracterul de endline va defini sfarsitul informatiei, si va insemna transmiterea informatiei
    if(size >= max_size){
      max_size = (max_size < 4) ? (max_size + 1) : (max_size / 2 * 3);
      buffer = realloc(buffer, max_size * sizeof(char));
    }
    buffer[size++] = ch;
  }
  
  if(size == 0){
    free(buffer);
    return NULL;
  }
  else{
    if(size >= max_size){
      max_size++;
      buffer = realloc(buffer, max_size * sizeof(char));     
    }
    buffer[size] = '\0';
  }
  return buffer;
}

unsigned char get_payload_size(char pack_type)
{
  return sizes[(int)pack_type];
}

char set_pack_type(char *buffer, unsigned char *pack_type)//la asezarea bitilor in acest pack_type trebuie sa fim atenti deoarece este specificat clar ca primii biti ne intereseaza.
{
  //fiind doar 7 valori de date inseamna ca vom avea doar dimensiuni pe 3 biti
  ssize_t size = strlen(buffer);
  if(size >= 128){//daca dimensiunea este mai mare decat maximul, va fi decupata informatia, iar functia va returna 0, altfel va returna 1
    *pack_type = (char)6;
    return 0;
  }
  else{
    for(ssize_t i = 0;i < 6;i++){
      if(size < sizes[i]){
	*pack_type = (char)(i + 1);
	break;
      }
    }
    return 1;
  }
}

void set_data(char *buffer,unsigned char *payload, ssize_t max_size)
{
  ssize_t actual_size = 0;
  for(;(buffer[actual_size] != '\0') && (actual_size < max_size) ;actual_size++){
    payload[actual_size] = buffer[actual_size];
  }
  
  for(;actual_size < max_size;actual_size++){
    payload[actual_size] = 0x00;
  }
}

unsigned char *pack_data(char *buffer)//Aceasta functie returneaza char, dar daorita aranjarii elementelor in memorie ii putem da typecast la tipul de date PACKET_T pentru a folosi structura specifica
{
  unsigned char *pack = malloc(148 * sizeof(char));
  memset(pack, 0x55, 7);
  memset(pack + 7, 0xff, 6);
  memset(pack + 13, 0x55, 6);
  if(set_pack_type(buffer, pack + 19) == 0){
    printf("Informatia va fi decupata pana la 128 caractere/bytes\n");
  }
  ssize_t size_payload = get_payload_size(pack[19]);
  set_data(buffer, pack + 20, size_payload);

  return pack;
}

void show_pack_data(char *pack)//Aceasta functie de afisare functioneaza si pe char dar si pe structura PACKET_T datorita modului de aranjare in memorie a bitilor, si dimensiunii unui char de un byte, dar si a tuturor campurilor din structura care sunt de tip char adica pe un byte
{
  for(ssize_t i = 0;i < get_payload_size(pack[19]) + 20;i++){
    printf("%c", pack[i]);
  }
  printf("\n");
  for(ssize_t i = 0;i < get_payload_size(pack[19]) + 20;i++){
    printf("%02x", pack[i]);
  }
  printf("\n");
}

char *unpack_data(char *pack)//functia de unpack preia tot char deoarece nu dorim sa blocam designul doar la structura respectiva
{
  char *buffer = NULL;
  UNPACK_STATE state = PREAMBLE_CHECK;
  ssize_t i = 0;
  //chit ca nu folosim nicaieri variabila de state, ea este asezata aici pentru a demonstra utilizarea ei intr-un automat de stare
  for(i = 0;i < 7;i++){
    if(pack[i] != 0x55){
      return NULL;
    }
  }
  state = PAYLOAD_READ;

  buffer = malloc((get_payload_size(pack[19]) + 1) * sizeof(char));

  for(i = 0;i < get_payload_size(pack[19]) - 1;i++){
    if(pack[i] != 0x00){
      buffer[i] = pack[i + 20];
    }
  }
  buffer[i] = '\0';
  return buffer; //vom adauga totul pana la intalnirea unui byte de 0x00 care evident ca nu semnifica informatie
}


unsigned char correct_n_number(const char *word1,const char *word2,const ssize_t letter_count)//functie care ofera numarul de greseli dintre 2 cuvinte pana la un maxim de letter_count litere
{
  ssize_t count = 0;
  ssize_t i = 0;
  for(i = 0;i < letter_count && (word1[i] != '\0' || word2[i] != '\0');i++){//In cazul in care intalnim sfarsitul unui cuvant vom considera restul spatiilor ca fiind diferente
    if(word1[i] != '\0' && word2[i] != '\0'){
      if(word1[i] != word2[i]){
	count++;
      }
    }
    else{
      count += letter_count - i;
      break;
    }
  }
  return count;
}

ssize_t get_word_position(char *output, char (*conditie)(const char *,const char *))//Aceasta functie ne va oferi cuvantul de pe o pozitie, in functie de conditia oferita ca parametru penbtru a putea geenraliza functia
{
  ssize_t word_position = 0;
  for(;word_position < 5;word_position++){
    if(conditie(DDR3[word_position], output)){
      return word_position;
    }
  }
  return -1;
}

char get_first_letter_same(const char *word1, const char *word2)//functie pentru utilizarea impreuna cu get_word_position pentru o conditie
{
  if(word1[0] == word2[0]){
    return 1;
  }
  return 0;
}

char get_word_by_weight(const char *word1, const char *word2)
{
  if(correct_n_number(word1, word2, word2[strlen(word2) + 1]) < word2[strlen(word2) + 2]){
    return 1;
  }
  return 0;
}

char *correct_word(char *buffer)//va primi un singur cuvant, exact cum va functiona si modulul nostru verilog, apoi il va returna corectat daca a intalnit o corectare
{
  /*Algoritmul se foloseste de ceva numit weight, care ar putea fi considerat intr-un fel o pondera, dar in cazul nostru nu am oferit pondere literelor in functie de asexzarea lor pe tasdtatura,
    sau de pozitia lor in sir, ci am generalizat, iar weight este doar numarul de diferente intalnite. Este un algoritm de baza, dar suficient de bun pentru necesitatile noastre, scopul fiind
    mai mult impkementarea pe placa in verilog, nu neaparat algoritmi extraordinar de complicati si buni */
  ssize_t position = 0;
  ssize_t weight = 0;
  ssize_t word_position = 0;
  ssize_t word_letter = 0;
  ssize_t inc = 0;
  WORD_STATE state; //Vom folosi un automat de stare pentru verificarea cuvantului
  //Acest automat de stare este pus pe stare initiala find_word_match unde va cauta dupa un cuvant cu prima litera lafel in memoria ddr3. Daca gaseste unul cu prima litera lafel
  //il va alege pe primul intalnit si va trece in modul check_word. Daca nu intalneste un astfel de cuvant, va primi prima valoare din memorie si va trece in modul weight_compare, in care va
  //ramane pana la sfarsit, deoarece nu mai este posibil sa nu avem nici o diferenta in cuvant. Aici ar fi foarte util o pondere a literelor.
  
    state = find_word_match;
    while(buffer[position] != '\0'){
      if(state == find_word_match){
	word_position = get_word_position(buffer, &get_first_letter_same);
	if(word_position == -1){
	  word_position = 0;
	  state = weight_compare;
	}
	  else{
	    state = check_word;
	  }
	}
      
      if(state == change_word){//In cazul in care este in modul change_word inseamna ca a intalnit o diferenta, si va incerca sa schimbe cuvantul cu unul care nu are nici o diferenta.
	if(buffer[position] > DDR3[word_position][word_letter]){//vom verifica daca litera diferita este mai mare sau mai mica, decat cea din cuvant pentru a stii daca vom cauta cuvantul
	  //in partea superioara a tabloului de valori sau in cea inferioara, datorita aranjarii cuvintelor in ordine alfabetica.
	    inc = 1;
	  }
	  else{
	    inc = -1;
	  }
	  if((inc == 1 && word_position < 5) || (inc == -1 && word_position > 0)){
	    if(strlen(DDR3[word_position + inc]) >= word_letter){//Verificam daca are macar dimensiunea la care ne aflam deja, deoarece in caz contrar nu putem compara numarul de greseli, si clar pana la
	      //final vom avea mai multe greseli
	      if(DDR3[word_position + inc][word_letter] == buffer[position]){//Verificam daca litera gresita este macar asemanatoare sau nu. Daca nu este nici nu mai incercam parcurgerea
	      if(correct_n_number(DDR3[word_position + inc], DDR3[word_position], word_letter) == 0){
		word_position += inc;
		state = check_word;
	      }
	    }
	  }
	  }
	if(state != check_word){
	  state = weight_compare;//Daca nu am reusit sa gasim un cuvant potrivit vom intra in modul weight_compare si vom ramane tot la acelasi cuvant, deoarece avem doar o eroare
	}
      }

      if(state == check_word){//In cazul modului check_word vom verifica daca exosta o diferenta intre litere, iar daca nu exista vom incrementa doar pozitile si modul ramane acelasi
	//Daca exista va trece in modul change_word
	 if(DDR3[word_position][word_letter] != buffer[position]){
	   state = change_word;
         }
	 else{
	   word_letter++;
	   position++;
	   if(DDR3[word_position][word_letter - 1] == '\0'){//Daca intalneste sfarsitul de cuvant vom scadea dimensiunea deoarece nu vrem sa folosim adrese nescrise. In verilog vom folosi un
	     //identificator de sfarsit de linie cum ar fi '\0' sau 0x00 sau orice poate peste valoarea 128 in decimal, deoarece in caz de signed char maxim este 128, iar ascii are maxim 128
	     //Noi vom avea grija deoarece pentru emojiuri vom folosi unsigned char, pentru a folosi coduri neutilizate deja de ascii normal
	     word_letter--;
	   }
	 }
       }
      
      if(state == weight_compare){//Cand intram in starea acceptoare weight_compare nu mai avem alta varianta de a iesi din aceasta, si se va decide daca avem sau nu diferente
	if(DDR3[word_position][word_letter] != buffer[position]){
	  weight++;
	  buffer[strlen(buffer) + 1] = position;//primul byte dupa delimitator l-am scris cu o valoare pentru utilizarea in functii.
	  buffer[strlen(buffer) + 2] = weight;//Al doilea lafel
	  ssize_t ant_val = 0;
	  ant_val = word_position;
	  word_position = get_word_position(buffer, &get_word_by_weight);//Verificam daca exista vreun cuvant care sa respecte conditia, adica sa aiba
	  //mai putine diferente decat cuvantul la care ne aflam acum. Daca exista il vom schimba, altfel ramanem la acelasi
	  if(word_position == -1){
	    word_position = ant_val;
	  }
	  else{
	    word_letter = (word_letter <= strlen(DDR3[word_position]) ? word_letter : strlen(DDR3[word_position]));
	    weight = correct_n_number(DDR3[word_position], buffer, word_letter);
	  }
	}
	position++;
	word_letter++;
	if(DDR3[word_position][word_letter - 1] == '\0'){
	  word_letter--;
	}
      }
    }
    if(word_letter < strlen(DDR3[word_position])  && weight > 0){//In cazul in care weight este mai mare decat 0, inseamna ca nu am reusit sa intlnim in dictionarul nostur un cuvant
      //care sa fie exact lafel cu unul din dictionar, pana la sfarsitul celui din dictionar cum ar fi : loveee -> love, asa ca vom adauga in weight si restul de litere care difera, adica sunt libere
      weight += strlen(DDR3[word_position]) - word_letter;
    }
    if((float)weight < (float)strlen(buffer)/2){//In cazul in care weight este mai mic decat 50% din lungimea cuvantului, cuvantul se va corecta cu cuvantul respectiv
    //De aici se poate evolua algoritmul la o medie ponderata, in functie de pozitia literei, prima avand cea mai mare pondere, iar restul pe rand scazand la pondere, dat fiind faptul ca doebicei
    //cea mai importnta litera ar trebui sa ne fie prima litera. Acest algoritm este doar unul de baza, pe un set scurt de date
    //Algoritmul ar putea salva undeva decizia de a corecta un cuvant, pentru ca apoi sa se calculeze probabilitatea ca acea coretare sa fie aleasa, la un anumit moment .
    //weight se refera la numarul de modificari.
      //folosim un tip de date float deoarece ne trebuie o precizie destul de buna, si nu ne putem multumi cu partea intreaga a unei impartiri, deoarece 3/2 = 1 si 3/2 = 1.5 sunt foarte diferite
      //in cazyuri ca acesta. Vom gasi in verilog o implementare pentru float daca nu exista. Probabil 4 biti pentru partea zecimala si 4 pentru cea fractionara, dar aceste valori vor fi in functie de
      //dimensiunea numerelor
    strcpy(buffer, DDR3[word_position]);
  }
  
  return buffer;
}

unsigned char *correct_words(const char *buffer)
{
  char word[30] = {0};
  char *output = malloc((strlen(buffer) * 2) * sizeof(char));//AM alocat incat sa avem memorie suficienta pentru memoria pe care o necesitam si la corectare.
  char *corrected_word = NULL;
  output[0] = '\0';//initializam stringul pentru a evita erorile
  ssize_t word_start = 0;
  ssize_t letter_pos = 0;
  while(word_start < strlen(buffer)){
    for(letter_pos = 0;(buffer[letter_pos + word_start] != ' ') && (buffer[letter_pos + word_start] != '\0');letter_pos++){//Parcurgem cuvantul byte cu byte pana la intalnirea spatilui, unde il vom folosi
      //in functia de corectare. Pana atunci se vor salva intr-un bloc de memorie
	word[letter_pos] = buffer[letter_pos + word_start];
      }
      
      while(buffer[letter_pos + word_start + 1] == ' '){
	letter_pos++;
      }
      
      word[letter_pos] = '\0';
      word[letter_pos + 1] = '\0';//Avem nevoie de cele 2 caractere in plus in functia de corectare a cuvintelor unde folosim ultimii 2 bytes de informatie pentru a salva diferite valori
      word[letter_pos + 2] = '\0';
      corrected_word = correct_word(word); //Transmitem cuvantul pentru corectare
      strcat(output, corrected_word);
      strcat(output, " ");
      //Pe masura ce corectam un cuvant il vom adauga intr-un buffer final, care va astepta pentru toate cuvintele sa fie verificate si salvate, pentru a-l putea trimite mai departe
      //in modulul de schimbare al cuvintelor
      word_start += letter_pos + 1;
    }
  return (unsigned char *)output;
}

char *change_word_neg(char *buffer)
{
  ssize_t pos = 0;
  ssize_t word_pos = 0;
  WORD_STATE state = find_word_match;
  while(buffer[pos] != '\0'){
    if(state == find_word_match){
      for(;word_pos < 5;word_pos++){
	if(correct_n_number(buffer, DDR3_2[0][word_pos], pos + 1) == 0){
	  state = check_word;
	  break;
	}
      }
      if(state != check_word){
	break;
      }
    }
    
    if(state == check_word){
      if(buffer[pos] != DDR3_2[0][word_pos][pos]){
	state = change_word;
      }
      else{
	if(buffer[pos + 1] == '\0'){
	  state = word_found;
	  break;
	}
	pos++;
      }
    }

    if(state == change_word){
      if(buffer[pos] < DDR3_2[0][word_pos][pos]){
	break;
      }
      else{
	for(;word_pos < 5;word_pos++){
	  if(correct_n_number(buffer, DDR3_2[0][word_pos], pos) == 0){
	    state = check_word;
	  }
	}
	break;
      }
    }
  }
  if(state == word_found){
      strcpy(buffer, DDR3_2[1][word_pos]);
      }
  return buffer;
}
    
unsigned char *change_content_for_pertrueder(char *buffer)
{
  char *aux_buffer = malloc(2 * strlen(buffer) * sizeof(char));
  aux_buffer[0] = '\0';
  char word[30];
  char *aux_word = NULL;
  ssize_t word_start = 0;
  ssize_t letter_pos = 0;
  while(word_start < strlen(buffer)){
    for(letter_pos = 0;(buffer[letter_pos + word_start] != ' ') && (buffer[letter_pos + word_start] != '\0');letter_pos++){
      word[letter_pos] = buffer[letter_pos + word_start];
    }
    word[letter_pos] = '\0';
    aux_word = change_word_neg(word);
    strcat(aux_buffer, aux_word);
    strcat(aux_buffer, " ");
    word_start += letter_pos + 1;
  }
  
  buffer = realloc(buffer, (strlen(aux_buffer) + 1) * sizeof(char));
  
  strcpy(buffer, aux_buffer);
  
  free(aux_buffer);
  return (unsigned char*)buffer;
}

int main(void)
{
  initial_begin();
  
  unsigned char *buffer = malloc(100 * sizeof(char));
  strcpy((char *)buffer, "love happy");
  buffer = change_content_for_pertrueder((char *)buffer);
  printf("%s\n", buffer);
  //unsigned char * output = correct_words(buffer);
  //printf("%s\n", output);
  
  //free(output);

  free(buffer);
  end_program();
  
  /*char *buffer = NULL;
  char *output = NULL;
  buffer = read_data();
  if(buffer == NULL){
    printf("No data to send\n");
  }
  else{
    PACKET_T *pack = (PACKET_T *)pack_data(buffer);
    show_pack_data((char *)pack);
    output = unpack_data((char *)pack);
    if(output == NULL){
      printf("Wrong preamble\n");
    }
    else{
      printf("%s\n", output);
    }
    free(buffer);
    free(output);
    free(pack);
    }*/
  return 0;
}
