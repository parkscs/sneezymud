#include "stdsneezy.h"
#include "database.h"
#include "session.cgi.h"

#include <map>
#include "sstring.h"

#include "cgicc/Cgicc.h"
#include "cgicc/HTTPHTMLHeader.h"
#include "cgicc/HTMLClasses.h"
#include <cgicc/HTTPCookie.h>
#include <cgicc/CgiEnvironment.h>
#include <cgicc/HTTPRedirectHeader.h>

#include <sys/types.h>
#include <md5.h>

using namespace cgicc;


void sendJavaScript();

void sendLogin();
void sendLoginCheck(Cgicc cgi, TSession);
void sendPickPlayer(int);
void sendMessageList(Cgicc cgi, int);

int main(int argc, char **argv)
{
  // trick the db code into using the prod database
  gamePort = PROD_GAMEPORT;
  toggleInfo.loadToggles();

  Cgicc cgi;
  form_iterator state_form=cgi.getElement("state");
  TSession session(cgi, "mudmail");

  if(!session.isValid()){
    if(state_form == cgi.getElements().end() ||
       **state_form != "logincheck"){
      sendLogin();
      return 0;
    } else {
      sendLoginCheck(cgi, session);
      return 0;
    }
  } else {
    if(state_form == cgi.getElements().end() || **state_form == "main"){
      sendPickPlayer(session.getAccountID());
      return 0;
    } else if(**state_form == "listmessages"){
      sendMessageList(cgi, session.getAccountID());
      return 0;
    } else if(**state_form == "logout"){
      session.logout();
      sendLogin();
      return 0;
    }

    cout << HTTPHTMLHeader() << endl;
    cout << html() << head() << title("Mudmail") << endl;
    cout << head() << body() << endl;
    cout << "Fell through state switch.<p><hr><p>" << endl;
    cout << body() << endl;
    cout << html() << endl;

    return 0;
  }  

  // shouldn't get here
  cout << HTTPHTMLHeader() << endl;
  cout << html() << head() << title("Mudmail") << endl;
  cout << head() << body() << endl;
  cout << "This is bad.<p><hr><p>" << endl;
  cout << body() << endl;
  cout << html() << endl;

}

void sendMessageList(Cgicc cgi, int account_id)
{
  int player_id=convertTo<int>(**(cgi.getElement("player")));
  TDatabase db(DB_SNEEZY);

  cout << HTTPHTMLHeader() << endl;
  cout << html() << head() << title("Mudmail") << endl;
  cout << head() << body() << endl;

  db.query("select m.mailfrom, m.timesent, m.content from mail m, player p where m.mailto=p.name and p.id=%i", player_id);
  
  cout << "<table border=1>" << endl;

  while(db.fetchRow()){
    cout << "<tr><td>" << db["mailfrom"] << endl;
    cout << "</td><td>" << db["timesent"] << endl;
    cout << "</td></tr><tr><td colspan=2>" << db["content"] << endl;
    cout << "</td></tr>" << endl;
  }

  cout << "</table>" << endl;

  cout << body() << endl;
  cout << html() << endl;
}

void sendLoginCheck(Cgicc cgi, TSession session)
{
  // validate, create session cookie + db entry, redirect to main
  TDatabase db(DB_SNEEZY);
  sstring name=**(cgi.getElement("account"));
  sstring passwd=**(cgi.getElement("passwd"));
  
  //// make sure the account exists, and pull out the encrypted password
  db.query("select account_id, passwd from account where name='%s'", 
	   name.c_str());
  
  if(!db.fetchRow()){
    cout << HTTPHTMLHeader() << endl;
    cout << html() << head() << title("Mudmail") << endl;
    cout << head() << body() << endl;
    cout << "Account not found.<p><hr><p>" << endl;
    cout << body() << endl;
    cout << html() << endl;
    return;
  }
  
  //// validate password
  sstring crypted=crypt(passwd.c_str(), name.c_str());
  
  // for some retarded reason the mud truncates the crypted portion to 10 chars
  if(crypted.substr(0,10) != db["passwd"]){
    cout << HTTPHTMLHeader() << endl;
    cout << html() << head() << title("Mudmail") << endl;
    cout << head() << body() << endl;
    cout << "Password incorrect.<p><hr><p>" << endl;
    cout << body() << endl;
    cout << html() << endl;
    return;
  }

  // ok, login is good, create session db entry and cookie
  session.createSession(convertTo<int>(db["account_id"]));
  
  cout << HTTPRedirectHeader("mudmail.cgi").setCookie(HTTPCookie("mudmail",session.getSessionID().c_str()));
  cout << endl;
  cout << html() << head() << title("Mudmail") << endl;
  cout << head() << body() << endl;
  cout << "Good job, you logged in.<p><hr><p>" << endl;
  cout << body() << endl;
  cout << html() << endl;
}


void sendLogin()
{
  cout << HTTPHTMLHeader() << endl;
  cout << html() << head() << title("Mudmail") << endl;
  cout << head() << body() << endl;

  cout << "<form action=\"mudmail.cgi\" method=post>" << endl;
  cout << "<table>" << endl;
  cout << "<tr><td>Account</td>" << endl;
  cout << "<td><input type=text name=account></td></tr>" << endl;
  cout << "<tr><td>Password</td>" << endl;
  cout << "<td><input type=password name=passwd></td></tr>" << endl;
  cout << "<tr><td colspan=2><input type=submit value=Login></td></tr>" <<endl;
  cout << "</table>" << endl;
  cout << "<input type=hidden name=state value=logincheck>" << endl;
  cout << "</form>" << endl;

  cout << body() << endl;
  cout << html() << endl;
}



void sendPickPlayer(int account_id)
{
  TDatabase db(DB_SNEEZY);

  cout << HTTPHTMLHeader() << endl;
  cout << html() << head() << title("Mudmail") << endl;
  sendJavaScript();
  cout << head() << body() << endl;
  cout << "<a href=\"mudmail.cgi?state=logout\">Logout</a><p>" << endl;

  // get a list of players in this account
  db.query("select p.id, p.name, count(m.*) as count from player p left outer join mail m on (p.name=m.mailto) where account_id=%i group by p.id, p.name order by p.name",
	   account_id);

  if(!db.fetchRow()){
    cout << "No players were found in this account." << endl;
    return;
  }

  cout << "<form action=\"mudmail.cgi\" method=post name=pickplayer>" << endl;
  cout << "<input type=hidden name=player>" << endl;
  cout << "<input type=hidden name=state value=listmessages>" << endl;
  cout << "<table border=1>" << endl;
  cout << "<tr><td>Player</td><td># of Letters</td></tr>";

  do {
    cout << "<tr><td>" << endl;
    cout << "<a href=javascript:pickplayer('" << db["id"] << "')>";
    cout << db["name"] << "</a></td><td align=center>" << endl;
    cout << db["count"] << "</td></tr>";
  } while(db.fetchRow());

  cout << "</form>" << endl;

  cout << body() << endl;
  cout << html() << endl;
}

void sendJavaScript()
{
  cout << "<script language=\"JavaScript\" type=\"text/javascript\">" << endl;
  cout << "<!--" << endl;

  // this function is for making links emulate submits in player selection
  cout << "function pickplayer(player)" << endl;
  cout << "{" << endl;
  cout << "document.pickplayer.player.value = player;" << endl;
  cout << "document.pickplayer.submit();" << endl;
  cout << "}" << endl;
  cout << "-->" << endl;
  cout << "</script>" << endl;
}





////////////////////////////







