#include "DBConnection.h"
#include "StrUtil.h"
#include "Error.h"
#include "MysqlStatementHandle.h"
#include "BaseAdaptor.h"

DBConnection *DBConnection_new(char *host, char *user, char *pass, 
                               char *dbname, unsigned int port) {
  DBConnection *dbc;
  MYSQL *mysql;

  if ((dbc = (DBConnection *)calloc(1,sizeof(DBConnection))) == NULL) {
    Error_write(ECALLERR,"DBConnection_new",ERR_SEVERE,"dbc");
    return NULL;
  }

  if ((mysql = mysql_init(NULL)) == NULL) {
    Error_write(EMYSQLCONN, "DBConnection_new", ERR_SEVERE,
                " failed creating mysql object in mysql_init, mysql error %s",mysql_error(mysql));
    return NULL;
  }

  if ((mysql_real_connect(mysql,host, user, pass, dbname, port, NULL, 0)) == NULL) {
    Error_write(EMYSQLCONN, "DBConnection_new", ERR_SEVERE,
                " dbname %s (host %s user %s pass %s port %d), mysql error %s",
                dbname,host,user,pass,port,mysql_error(mysql));
    return NULL;
  }

  StrUtil_copyString(&(dbc->host), host, 0);
  StrUtil_copyString(&(dbc->user), user, 0);
  if (pass) StrUtil_copyString(&(dbc->pass), pass, 0);
  StrUtil_copyString(&(dbc->dbname), dbname, 0);

  dbc->mysql   = mysql;
  dbc->prepare = DBConnection_prepare;

  if (!dbc->host   || 
      !dbc->user   || 
      !dbc->dbname ||
      (pass && !dbc->pass)) {
    Error_trace("DBConnnection_new",NULL);
    return NULL;
  } 
 
  return dbc;
}

StatementHandle *DBConnection_prepare(DBConnection *dbc, char *queryStr, int queryLen) {
/*
  mysql_real_query(dbc->mysql, queryStr, queryLen);
  return mysql_store_result(dbc->mysql);
*/
  return (StatementHandle *)MysqlStatementHandle_new(dbc,queryStr);
}

BaseAdaptor *DBConnection_getAdaptor(DBConnection *dbc, int type) {
  int i;
  BaseAdaptor *ad = NULL;

  for (i=0; i<dbc->nAdaptor && !ad; i++) {
    if (dbc->adaptors[i]->adaptorType == type) {
      ad = dbc->adaptors[i];
    }
  }
  if (!ad) {
    //fprintf(stderr,"ERROR: No adaptor of type %s\n",Adaptor_TypeStrings[type]);
  }
  return ad;
}

int DBConnection_addAdaptor(DBConnection *dbc, BaseAdaptor *ba) {
  if (!dbc->nAdaptor || !(dbc->nAdaptor%10)) {
    if ((dbc->adaptors = (BaseAdaptor **)realloc(dbc->adaptors,(dbc->nAdaptor+10)*sizeof(BaseAdaptor *))) == NULL) {
      fprintf(stderr,"ERROR: Failed allocating adaptor array\n");
      return 0;
    }
  }

  dbc->adaptors[dbc->nAdaptor++] = ba;

  return 1;
}

/*
=head2 from_date_to_seconds

  Arg [1]    : date $date
  Example    : my $string = $dbc->from_date_to_seconds($date);
  Description: Giving a string representing a column of type date
                applies the database function to convert to the number of seconds from 01-01-1970
  Returntype : string
  Exceptions : none
  Caller     : general
  Status     : at risk

=cut
*/
void DBConnection_fromDateToSeconds(DBConnection *dbc, char *column, char *wrappedColumn) {
  if (!strcmp(DBConnection_getDriverName(dbc), "mysql")) {
    sprintf(wrappedColumn, "UNIX_TIMESTAMP(%s)", column);
    return;
  }
/* Who cares!
  elsif ($self->driver eq 'odbc'){
        $string = "DATEDIFF(second,'JAN 1 1970',$column)";
    }
    elsif ($self->driver eq 'SQLite'){
        $string = "STRFTIME('%s', $column)";
    }
*/
  else {
    fprintf(stderr, "Not possible to convert %s due to an unknown database driver: %s - bye\n", column, DBConnection_getDriverName(dbc));
// Huh why not just die - this isn't a good thing to happen 
    //return '';
    exit(1);
  }

  return; // wrappedColumn is returned filled with the required string
}

// Fixed to mysql for now
char *DBConnection_getDriverName(DBConnection *dbc) {
  return "mysql";
} 
