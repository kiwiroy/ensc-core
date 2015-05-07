/*
 * Copyright [1999-2015] Wellcome Trust Sanger Institute and the EMBL-European Bioinformatics Institute
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>

#include "DBAdaptor.h"
#include "EnsC.h"

#include "BaseTest.h"

int main(int argc, char *argv[]) {
  DBAdaptor *dba;

  initEnsC(argc, argv);

  dba = DBAdaptor_new("ensembldb.ensembl.org","anonymous",NULL,"homo_sapiens_core_29_35b",3306,NULL);

  ok(1,!strcmp("NCBI35",DBAdaptor_getAssemblyType(dba)));

  return 0;
}
