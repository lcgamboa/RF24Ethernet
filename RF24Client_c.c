/*
 RF24Client.cpp - Arduino implementation of a uIP wrapper class.
 Copyright (c) 2016 Luis Claudio Gamboa Lopes <lcgamboa@yahoo.com>
 Copyright (c) 2014 tmrh20@gmail.com, github.com/TMRh20 
 Copyright (c) 2013 Norbert Truchsess <norbert.truchsess@t-online.de>
 All rights reserved.

 Stream.cpp - adds parsing methods to Stream class
 Copyright (c) 2008 David A. Mellis.  All right reserved.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
  */
#include "RF24Ethernet_c.h"

#define UIP_TCP_PHYH_LEN UIP_LLH_LEN+UIP_IPTCPH_LEN

uip_userdata_t all_data[UIP_CONNS];


/*************************************************************/

void RF24EC_init(RF24Client* cli){
 cli->data=NULL;	
 cli->_timeout=1000;
}

/*************************************************************/

void RF24EC_init_d(RF24Client* cli, uip_userdata_t* conn_data){
 cli->data=conn_data;
 cli->_timeout=1000;
}

/*************************************************************/

uint8_t RF24EC_connected(RF24Client* cli){
  return (cli->data && (cli->data->packets_in != 0 || (cli->data->state & UIP_CLIENT_CONNECTED))) ? 1 : 0;
}

/*************************************************************/

int RF24EC_connect(RF24Client* cli, IPAddress ip, uint16_t port) {

#if UIP_ACTIVE_OPEN > 0
 
  //uint32_t timer = millis();

//do{

  RF24EC_stop(cli);
  uip_ipaddr_t ipaddr;
  uip_ip_addr(ipaddr, ip);

  struct uip_conn* conn = uip_connect(&ipaddr, htons(port));

  if (conn){

	#if UIP_CONNECT_TIMEOUT > 0
    int32_t timeout = millis() + 1000 * UIP_CONNECT_TIMEOUT;
    #endif

    while((conn->tcpstateflags & UIP_TS_MASK) != UIP_CLOSED) {
      RF24E_tick(&RF24Ethernet);
	
	  if ((conn->tcpstateflags & UIP_TS_MASK) == UIP_ESTABLISHED) {
	    cli->data = (uip_userdata_t*) conn->appstate;
	    IF_RF24ETHERNET_DEBUG_CLIENT( Serial.print(millis()); Serial.print(F(" connected, state: ")); Serial.print(data->state); Serial.print(F(", first packet in: ")); Serial.println(data->packets_in);  );
	    return 1;
	  }	  
	
	#if UIP_CONNECT_TIMEOUT > 0
	  if (((int32_t)(millis() - timeout)) > 0) {
	    conn->tcpstateflags = UIP_CLOSED;
	    break;
	  }
	#endif
    
	}
  }
  //delay(25);
//}while(millis()-timer < 175);

#endif //Active open enabled

return 0;
}

/*************************************************************/

int RF24EC_connect_h(RF24Client* cli, const char *host, uint16_t port) {
  // Look up the host first
  int ret = 0;

  #if UIP_UDP
  DNSClient dns;
  IPAddress remote_addr;

  dns.begin(RF24EthernetClass::_dnsServerAddress);
  ret = dns.getHostByName(host, remote_addr);
  
  if (ret == 1) {
    #if defined (ETH_DEBUG_L1) || defined (RF24ETHERNET_DEBUG_DNS)
	  Serial.println(F("*UIP Got DNS*"));
	#endif
    return connect(remote_addr, port);
  }
  #endif
  #if defined (ETH_DEBUG_L1) || defined (RF24ETHERNET_DEBUG_DNS)
    Serial.println(F("*UIP DNS fail*"));
  #endif

  return ret;
}

/*************************************************************/

void RF24EC_stop(RF24Client* cli) {

  if (cli->data && cli->data->state) {
      
	IF_RF24ETHERNET_DEBUG_CLIENT( Serial.print(millis()); Serial.println(F(" before stop(), with data")); );
      
	cli->data->packets_in = 0;
	cli->data->dataCnt = 0;
	  
    if (cli->data->state & UIP_CLIENT_REMOTECLOSED){
      cli->data->state = 0;
    }else{
      cli->data->state |= UIP_CLIENT_CLOSE;
    }
	  
	IF_RF24ETHERNET_DEBUG_CLIENT( Serial.println(F("after stop()")); );
    
  }else{
    IF_RF24ETHERNET_DEBUG_CLIENT( Serial.print(millis()); Serial.println(F(" stop(), data: NULL")); );
  }
	
  cli->data = NULL;
  RF24E_tick(&RF24Ethernet);
}

/*************************************************************/

// the next function allows us to use the client returned by
// EthernetServer::available() as the condition in an if-statement.
//bool RF24EC_operator==(const RF24Client& rhs) {
//  return data && rhs.data && (data == rhs.data);
//}

/*************************************************************/

//RF24EC_operator bool() {
//  RF24E_tick(&RF24Ethernet);
//  return data && (!(data->state & UIP_CLIENT_REMOTECLOSED) || data->packets_in != 0);
//}
    

int RF24EC_valid(RF24Client *cli)
{
   RF24E_tick(&RF24Ethernet);
  return cli->data && (!(cli->data->state & UIP_CLIENT_REMOTECLOSED) || cli->data->packets_in != 0);
}  

/*************************************************************/

size_t RF24EC_write_b(RF24Client* cli, uint8_t c) {
  return RF24EC__write(cli,cli->data, &c, 1);
}

/*************************************************************/

size_t RF24EC_write(RF24Client* cli, const uint8_t *buf, size_t size) {
  return RF24EC__write(cli,cli->data, buf, size);
}

/*************************************************************/
size_t RF24EC_write_s(RF24Client* cli, const char *str) {
      if (str == NULL) return 0;
      return RF24EC_write(cli,(const uint8_t *)str, strlen(str));
}
    

/*************************************************************/

size_t RF24EC__write(RF24Client* cli, uip_userdata_t* u, const uint8_t *buf, size_t size) {

  size_t total_written = 0;
  size_t payloadSize = rf24_min(size,UIP_TCP_MSS);
  //Serial.println("W1");

  //uint32_t testTimeout=millis();
  
test2:    

  RF24E_tick(&RF24Ethernet);
  if (u && !(u->state & (UIP_CLIENT_CLOSE | UIP_CLIENT_REMOTECLOSED ) ) && u->state & (UIP_CLIENT_CONNECTED) ) {

    if( u->out_pos + payloadSize > UIP_TCP_MSS || u->hold){
	  goto test2;
    }

	IF_RF24ETHERNET_DEBUG_CLIENT( Serial.println(); Serial.print(millis()); Serial.print(F(" UIPClient.write: writePacket(")); Serial.print(u->packets_out); Serial.print(F(") pos: ")); Serial.print(u->out_pos); Serial.print(F(", buf[")); Serial.print(size-total_written); Serial.print(F("]: '")); Serial.write((uint8_t*)buf+total_written,payloadSize); Serial.println(F("'")); );
	
	memcpy(u->myData+u->out_pos,buf+total_written,payloadSize);	
	u->packets_out = 1;
	u->out_pos+=payloadSize;

	total_written += payloadSize;
	
	if( total_written < size ){	
		size_t remain = size-total_written;
		payloadSize = rf24_min(remain,UIP_TCP_MSS);
		
		//RF24EthernetClass::tick();
		goto test2;
	}
	return u->out_pos;
	}
  u->hold = false;
  return -1;
}

/*************************************************************/

void uip_log(RF24Client* cli, char* msg ){
	//Serial.println();
	//Serial.println("** UIP LOG **");
	//Serial.println(msg);
}

/*************************************************************/

//void serialip_appcall(RF24Client* cli) {
void serialip_appcall() {
  
  uip_userdata_t *u = (uip_userdata_t*)uip_conn->appstate;
  
  /*******Connected**********/
  if (!u && uip_connected()){
  
	IF_RF24ETHERNET_DEBUG_CLIENT( Serial.println(); Serial.print(millis()); Serial.println(F(" UIPClient uip_connected")); );

    u = (uip_userdata_t*) RF24EC__allocateData();
    
	if (u) {
      uip_conn->appstate = u;
      IF_RF24ETHERNET_DEBUG_CLIENT( Serial.print(F("UIPClient allocated state: ")); Serial.println(u->state,BIN); );
    }else{
      IF_RF24ETHERNET_DEBUG_CLIENT(Serial.println(F("UIPClient allocation failed")); );
	}
  }
  
  /*******User Data RX**********/
  if(u){
    if (uip_newdata()){
      IF_RF24ETHERNET_DEBUG_CLIENT( Serial.println(); Serial.print(millis()); Serial.print(F(" UIPClient uip_newdata, uip_len:")); Serial.println(uip_len); );
      
      if(u->sent){
          u->hold =false;
	  u->out_pos=false;
	  u->windowOpened=false;
	  u->packets_out= false;
      }
	  if (uip_len && !(u->state & (UIP_CLIENT_CLOSE | UIP_CLIENT_REMOTECLOSED))){
		  
		uip_stop();
		u->state &= ~UIP_CLIENT_RESTART;
		u->windowOpened = false;	
        u->restartTime = millis();		
		u->connAbortTime = u->restartTime;
	    memcpy(&u->myDataIn[u->dataPos+u->dataCnt], uip_appdata, uip_datalen());
	    u->dataCnt = u->dataCnt+uip_datalen();
		
	    u->packets_in = 1;
		  
	  }		  
	  goto finish;
	}

  /*******Closed/Timed-out/Aborted**********/  
  // If the connection has been closed, save received but unread data.
  if (uip_closed() || uip_timedout() || uip_aborted()) {
    
	IF_RF24ETHERNET_DEBUG_CLIENT( Serial.println();Serial.print(millis()); Serial.println(F(" UIPClient uip_closed")); );
	// drop outgoing packets not sent yet:
	u->packets_out = 0;
    
	if (u->packets_in) {
	  ((uip_userdata_closed_t *)u)->lport = uip_conn->lport;
	  u->state |= UIP_CLIENT_REMOTECLOSED;
	  IF_RF24ETHERNET_DEBUG_CLIENT( Serial.println(F("UIPClient close 1")); );
	}else{
	  IF_RF24ETHERNET_DEBUG_CLIENT( Serial.println(F("UIPClient close 2")); );
	  u->state = 0;
	}

	IF_RF24ETHERNET_DEBUG_CLIENT( Serial.println(F("after UIPClient uip_closed")); );
	uip_conn->appstate = NULL;	
	goto finish;
  }

  /*******ACKED**********/  
  if (uip_acked()) {
    IF_RF24ETHERNET_DEBUG_CLIENT( Serial.println(); Serial.print(millis()); Serial.println(F(" UIPClient uip_acked")); );
	u->state &= ~UIP_CLIENT_RESTART;
	u->hold=false;
       	u->out_pos=false; 
	u->windowOpened=false;
       	u->packets_out=false;
    u->restartTime = millis();    
	u->connAbortTime = u->restartTime;	
  }
	
  /*******Polling**********/
  if (uip_poll() || uip_rexmit() ){
    //IF_RF24ETHERNET_DEBUG_CLIENT( Serial.println(); Serial.println(F("UIPClient uip_poll")); );
    
	if (u->packets_out != 0 ) {
	  uip_len = u->out_pos;
	  uip_send(u->myData,u->out_pos);
	  u->hold = true;
      u->sent = true;
	  goto finish;    
	}else
    // Restart mechanism to keep connections going
	// Only call this if the TCP window has already been re-opened, the connection is being polled, but no data
	// has been acked
	if( !(u->state & (UIP_CLIENT_CLOSE | UIP_CLIENT_REMOTECLOSED)) ){
	  
	  if( u->windowOpened == true && u->state & UIP_CLIENT_RESTART && millis() - u->restartTime > u->restartInterval ){		
		u->restartTime = millis();		    
		// Abort the connection if the connection is dead after a set timeout period (uip-conf.h)
		#if defined UIP_CONNECTION_TIMEOUT
		
		if(millis() - u->connAbortTime >= UIP_CONNECTION_TIMEOUT){
		  #if defined RF24ETHERNET_DEBUG_CLIENT || defined ETH_DEBUG_L1
		  Serial.println(); Serial.print(millis()); Serial.println(F(" *********** ABORTING CONNECTION ***************"));
		  #endif
		  u->windowOpened = false;
		  u->state = 0;
		  uip_conn->appstate = NULL;
		  uip_abort();
		  goto finish;	      
		}else{
		#endif
		#if defined RF24ETHERNET_DEBUG_CLIENT || defined ETH_DEBUG_L1
		  Serial.println(); Serial.print(millis()); Serial.print(F(" UIPClient Re-Open TCP Window, time remaining before abort: ")); Serial.println((UIP_CONNECTION_TIMEOUT - (millis() - u->connAbortTime)) / 1000.00);
		#endif
		  u->restartInterval=u->restartInterval+500;
		  u->restartInterval=rf24_min(u->restartInterval,7000);
		  uip_restart();
		#if defined UIP_CONNECTION_TIMEOUT
	    }
		#endif
	  }
	}
  }


  /*******Close**********/	
	
  if (u->state & UIP_CLIENT_CLOSE) {
    IF_RF24ETHERNET_DEBUG_CLIENT( Serial.println(); Serial.print(millis()); Serial.println(F(" UIPClient state UIP_CLIENT_CLOSE")); );

	if (u->packets_out == 0) {
      u->state = 0;
      uip_conn->appstate = NULL;
      uip_close();
      IF_RF24ETHERNET_DEBUG_CLIENT( Serial.println(F("no blocks out -> free userdata")); );

    }else{
      uip_stop();
      IF_RF24ETHERNET_DEBUG_CLIENT( Serial.println(F("blocks outstanding transfer -> uip_stop()")); );
    }
  }			
finish:;
 /*
finish_newdata:
  if (u->state & UIP_CLIENT_RESTART && !u->windowOpened) {
    if( !(u->state & (UIP_CLIENT_CLOSE | UIP_CLIENT_REMOTECLOSED))){	  
	  uip_restart();
	  #if defined ETH_DEBUG_L1
	    Serial.println(); Serial.print(millis()); 
		Serial.println(F(" UIPClient Re-Open TCP Window"));
      #endif
	  u->windowOpened = true;
	  u->restartInterval = UIP_WINDOW_REOPEN_DELAY; //.75 seconds
	  u->restartTime = u->connAbortTime = millis();
	}
  }
  */
  }
  


}

/*******************************************************/


uip_userdata_t *RF24EC__allocateData() {
  uint8_t sock;	
  for ( sock = 0; sock < UIP_CONNS; sock++ ) {
	uip_userdata_t* data = &all_data[sock];
    if (!data->state) {
	  data->state = sock | UIP_CLIENT_CONNECTED;
	  data->packets_in=0;
	  data->packets_out=0;
	  data->dataCnt = 0;
	  data->dataPos=0;
	  data->out_pos = 0;
      data->hold=0;
	  return data;
	}
  }
  return NULL;
}

int RF24EC_waitAvailable(RF24Client* cli, uint32_t timeout){

	uint32_t start = millis();
    while(RF24EC_available(cli) < 1){	
		if(millis()-start > timeout){
		  return 0; 
		}
                RF24E_tick(&RF24Ethernet);
	}
	return RF24EC_available(cli);
}

/*************************************************************/

int RF24EC_available(RF24Client* cli) {
  
  RF24E_tick(&RF24Ethernet);
  if (cli){	
	return RF24EC__available(cli,cli->data);
  }
  return 0;
}

/*************************************************************/

int RF24EC__available(RF24Client* cli, uip_userdata_t *u) {
  if(u->packets_in){
	return u->dataCnt;
  }
  return 0;
}

int RF24EC_read_b(RF24Client* cli, uint8_t *buf, size_t size) {

  if (cli) {

    if (!cli->data->packets_in) { return -1; }

    size = rf24_min(cli->data->dataCnt,size);
	memcpy(buf,&cli->data->myDataIn[cli->data->dataPos],size);
	cli->data->dataCnt -= size;
	
	cli->data->dataPos+=size;
	
	if(!cli->data->dataCnt) {
      
	  cli->data->packets_in = 0;
	  cli->data->dataPos = 0;
	  
      if (uip_stopped(&uip_conns[cli->data->state & UIP_CLIENT_SOCKETS]) && !(cli->data->state & (UIP_CLIENT_CLOSE | UIP_CLIENT_REMOTECLOSED))){
        cli->data->state |= UIP_CLIENT_RESTART;
		cli->data->restartTime = 0;
		
		IF_ETH_DEBUG_L2( Serial.print(F("UIPClient set restart ")); Serial.println(data->state & UIP_CLIENT_SOCKETS); Serial.println(F("**")); Serial.println(data->state,BIN); Serial.println(F("**")); Serial.println(UIP_CLIENT_SOCKETS,BIN); Serial.println(F("**"));  );
	  }else{
		IF_ETH_DEBUG_L2( Serial.print(F("UIPClient stop?????? ")); Serial.println(data->state & UIP_CLIENT_SOCKETS); Serial.println(F("**")); Serial.println(data->state,BIN); Serial.println(F("**")); Serial.println(UIP_CLIENT_SOCKETS,BIN); Serial.println(F("**"));  );
			  
	  }
	  
      if (cli->data->packets_in == 0) {
        if (cli->data->state & UIP_CLIENT_REMOTECLOSED) {
          cli->data->state = 0;
          cli->data = NULL;
        }        
      }
    }
    return size;
  }
  
  return -1;
}

/*************************************************************/

int RF24EC_read(RF24Client* cli) {
  uint8_t c;
  if (RF24EC_read_b(cli,&c,1) < 0)
    return -1;
  return c;
}

/*************************************************************/

int RF24EC_peek(RF24Client* cli) {
	if(RF24EC_available(cli)){
	  return cli->data->myDataIn[cli->data->dataPos];
	}
  return -1;
}

/*************************************************************/

void RF24EC_flush(RF24Client* cli) {
  if (cli) {
	cli->data->packets_in = 0;
	cli->data->dataCnt = 0;
  }
}

/*************************************************************/
bool RF24EC_findUntil(RF24Client* cli, char * target,char * terminator)
{
return RF24EC_findUntil_f(cli, target, strlen(target), terminator, strlen(terminator));
}
       
/*************************************************************/
bool RF24EC_find(RF24Client* cli, char * target)
{
  return RF24EC_findUntil_f(cli, target, strlen(target), NULL, 0);
}
/*************************************************************/

bool RF24EC_findUntil_f(RF24Client* cli, char *target, size_t targetLen, char *terminator, size_t termLen)
{
  if (terminator == NULL) {
    MultiTarget t[1];
    t[0].str=target;
    t[0].len=targetLen;
    t[0].index=0;
    return RF24EC_findMulti(cli,t, 1) == 0 ? true : false;
  } else {
    MultiTarget t[2];
    t[0].str=target;
    t[0].len=targetLen;
    t[0].index=0;
    t[1].str=terminator;
    t[1].len=termLen;
    t[1].index=0;
    return RF24EC_findMulti(cli,t, 2) == 0 ? true : false;
  }
}
/*************************************************************/
long RF24EC_parseInt(RF24Client* cli, LookaheadMode_ lookahead, char ignore)
{
  bool isNegative = false;
  long value = 0;
  int c;

  c = RF24EC_peekNextDigit(cli,lookahead, false);
  // ignore non numeric leading characters
  if(c < 0)
    return 0; // zero returned if timeout

  do{
    if(c == ignore)
      ; // ignore this character
    else if(c == '-')
      isNegative = true;
    else if(c >= '0' && c <= '9')        // is c a digit?
      value = value * 10 + c - '0';
    RF24EC_read(cli);  // consume the character we got with peek
    c = RF24EC_timedPeek(cli);
  }
  while( (c >= '0' && c <= '9') || c == ignore );

  if(isNegative)
    value = -value;
  return value;
}
/*************************************************************/
int RF24EC_findMulti(RF24Client* cli, MultiTarget *targets, int tCount) {
  // any zero length target string automatically matches and would make
  // a mess of the rest of the algorithm.
  MultiTarget *t;
  for (t = targets; t < targets+tCount; ++t) {
    if (t->len <= 0)
      return t - targets;
  }

  while (1) {
    int c = RF24EC_timedRead(cli);
    if (c < 0)
      return -1;

    for (t = targets; t < targets+tCount; ++t) {
      // the simple case is if we match, deal with that first.
      if (c == t->str[t->index]) {
        if (++t->index == t->len)
          return t - targets;
        else
          continue;
      }

      // if not we need to walk back and see if we could have matched further
      // down the stream (ie '1112' doesn't match the first position in '11112'
      // but it will match the second position so we can't just reset the current
      // index to 0 when we find a mismatch.
      if (t->index == 0)
        continue;

      int origIndex = t->index;
      do {
        --t->index;
        // first check if current char works against the new current index
        if (c != t->str[t->index])
          continue;

        // if it's the only char then we're good, nothing more to check
        if (t->index == 0) {
          t->index++;
          break;
        }

        // otherwise we need to check the rest of the found string
        int diff = origIndex - t->index;
        size_t i;
        for (i = 0; i < t->index; ++i) {
          if (t->str[i] != t->str[i + diff])
            break;
        }

        // if we successfully got through the previous loop then our current
        // index is good.
        if (i == t->index) {
          t->index++;
          break;
        }

        // otherwise we just try the next index
      } while (t->index);
    }
  }
  // unreachable
  return -1;
}

int RF24EC_peekNextDigit(RF24Client* cli, LookaheadMode_ lookahead, bool detectDecimal)
{
  int c;
  while (1) {
    c = RF24EC_timedPeek(cli);

    if( c < 0 ||
        c == '-' ||
        (c >= '0' && c <= '9') ||
        (detectDecimal && c == '.')) return c;

    switch( lookahead ){
        case SKIP_NONE_: return -1; // Fail code.
        case SKIP_WHITESPACE_:
            switch( c ){
                case ' ':
                case '\t':
                case '\r':
                case '\n': break;
                default: return -1; // Fail code.
            }
        case SKIP_ALL_:
            break;
    }
    RF24EC_read(cli);  // discard non-numeric
  }
  return -1;
}

// private method to read stream with timeout
int RF24EC_timedRead(RF24Client* cli)
{
  int c;
  cli->_startMillis = millis();
  do {
    c = RF24EC_read(cli);
    if (c >= 0) return c;
  } while(millis() - cli->_startMillis < cli->_timeout);
  return -1;     // -1 indicates timeout
}


int RF24EC_timedPeek(RF24Client* cli)
{
  int c;
  cli->_startMillis = millis();
  do {
    c = RF24EC_peek(cli);
    if (c >= 0) return c;
  } while(millis() - cli->_startMillis < cli->_timeout);
  return -1;     // -1 indicates timeout
}
