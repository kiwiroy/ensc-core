/*
=head1 DESCRIPTION

The ChainedAssemblyMapper is an extension of the regular AssemblyMapper
that allows for mappings between coordinate systems that require
multi-step mapping.  For example if explicit mappings are defined
between the following coordinate systems,

  chromosome <-> contig
  contig     <-> clone

the ChainedAssemblyMapper would be able to perform implicit mapping
between the chromosome and clone coordinate systems.  This should be
transparent to the user of this module, and users should not even
realise that they are using a chained assembly mapper as opposed to a
normal assembly mapper.

=head1 METHODS

=cut
*/


/*
my $FIRST = 'first';
my $MIDDLE = 'middle';
my $LAST  = 'last';
*/

// 2^20 = approx 10^6
int CHUNKFACTOR = 20;

// max size of the pair cache in the mappers
int DEFAULT_MAX_PAIR_COUNT = 6000;

/*
=head2 new

  Arg [1]    : Bio::EnsEMBL::DBSQL::AssemblyMapperAdaptor
  Arg [2]    : Bio::EnsEMBL::CoordSystem $src_cs
  Arg [3]    : Bio::EnsEMBL::CoordSystem $int_cs
  Arg [4]    : Bio::EnsEMBL::CoordSystem $dst_cs
  Example    : Should use AssemblyMapperAdaptor->fetch_by_CoordSystems
  Description: Creates a new AssemblyMapper
  Returntype : Bio::EnsEMBL::DBSQL::AssemblyMapperAdaptor
  Exceptions : thrown if wrong number of coord_systems are provided
  Caller     : AssemblyMapperAdaptor
  Status     : Stable

=cut
*/

ChainedAssemblyMapper *ChainedAssemblyMapper_new(AssemblyMapperAdaptor *adaptor, Vector *coordSystems) {
  ChainedAssemblyMapper *cam;

  if ((cam = (ChainedAssemblyMapper *)calloc(1, sizeof(ChainedAssemblyMapper))) == NULL) {
    fprintf(stderr, "ERROR: Failed allocating space for ChainedAssemblyMapper\n");
    return NULL;
  }

  ChainedAssemblyMapper_setAdaptor(cam, adaptor);

  AssemblyMapperAdaptor_cacheSeqIdsWithMultAssemblies(adaptor);

  // Set the component, intermediate and assembled coordinate systems
  if ( Vector_getNumElement(coordSystems) != 3 ) {
    fprintf(stderr, "Can only map between two coordinate systems %d were provided\n", Vector_getNumElement(coordSystems));
    exit(1);
  }

  ChainedAssemblyMapper_setFirstCoordSystem(cam, Vector_getElementAt(coordSystems, 0));
  ChainedAssemblyMapper_setMiddleCoordSystem(cam, Vector_getElementAt(coordSystems, 1));
  ChainedAssemblyMapper_setLastCoordSystem(cam, Vector_getElementAt(coordSystems, 2));

  // maps between first and intermediate coord systems
  Mapper *firstMidMapper = Mapper_new("first", "middle");
  ChainedAssemblyMapper_setFirstMiddleMapper(cam, firstMidMapper);

  // maps between last and intermediate
  Mapper *lastMidMapper = Mapper_new("last", "middle");
  ChainedAssemblyMapper_setLastMiddleMapper(cam, lastMidMapper);

  // mapper that is actually used and is loaded by the mappings generated
  // by the other two mappers
  Mapper *firstLastMapper = Mapper_new("first", "last", srcSc, dstCs); //NIY Extra args in Mapper_new??
  ChainedAssemblyMapper_setFirstLastMapper(cam, firstLastMapper);

  // need registries to keep track of what regions are registered in source
  // and destination coordinate systems
  ChainedAssemblyMapper_setFirstRegistry(cam, RangeRegistry_new());
  ChainedAssemblyMapper_setLastRegistry(cam, RangeRegistry_new());

  ChainedAssemblyMapper_setMaxPairCount(cam, DEFAULT_MAX_PAIR_COUNT);

  return cam;
}


/*
=head2 max_pair_count

  Arg [1]    : (optional) int $max_pair_count
  Example    : $mapper->max_pair_count(100000)
  Description: Getter/Setter for the number of mapping pairs allowed in the
               internal cache. This can be used to override the default value
               (6000) to tune the performance and memory usage for certain
               scenarios. Higher value = bigger cache, more memory used
  Returntype : int
  Exceptions : none
  Caller     : general
  Status     : Stable

=cut
*/

sub max_pair_count {
  my $self = shift;
  $self->{'max_pair_count'} = shift if(@_);
  return $self->{'max_pair_count'};
}




/*
=head2 register_all

  Arg [1]    : none
  Example    : $mapper->max_pair_count(10e6);
               $mapper->register_all();
  Description: Pre-registers all assembly information in this mapper.  The
               cache size should be set to a sufficiently large value
               so that all of the information can be stored.  This method
               is useful when *a lot* of mapping will be done in regions
               which are distributed around the genome.   After registration
               the mapper will consume a lot of memory but will not have to
               perform any SQL and will be faster.
  Returntype : none
  Exceptions : none
  Caller     : specialised programs doing a lot of mapping
  Status     : Stable

=cut
*/

void ChainedAssemblyMapper_registerAll(ChainedAssemblyMapper *cam) {
  AssemblyMapperAdaptor *ama = ChainedAssemblyMapper_getAdaptor(cam);

  AssemblyMapperAdaptor_registerAllChained(ama, cam);

  return;
}



void ChainedAssemblyMapper_flush(ChainedAssemblyMapper *cam) {

  RangeRegistry_flush( ChainedAssemblyMapper_getFirstRegistry(cam) );
  RangeRegistry_flush( ChainedAssemblyMapper_getLastRegistry(cam) );

  Mapper_flush( ChainedAssemblyMapper_getFirstMiddleMapper(cam) );
  Mapper_flush( ChainedAssemblyMapper_getLastMiddleMapper(cam) );
  Mapper_flush( ChainedAssemblyMapper_getFirstLastMapper(cam) );

  return;
}

/*
=head2 size

  Args       : none
  Example    : $num_of_pairs = $mapper->size();
  Description: return the number of pairs currently stored.
  Returntype : int
  Exceptions : none
  Caller     : general
  Status     : Stable

=cut
*/

int ChainedAssemblyMapper_getSize(ChainedAssemblyMapper *cam) {
  return ( Mapper_getPairCount( ChainedAssemblyMapper_getFirstLastMapper(cam) ) +
           Mapper_getPairCount( ChainedAssemblyMapper_getLastMiddleMapper(cam) ) +
           Mapper_getPairCount( ChainedAssemblyMapper_getFirstMiddleMapper(cam) ) );
}



/*
=head2 map

  Arg [1]    : string $frm_seq_region
               The name of the sequence region to transform FROM
  Arg [2]    : int $frm_start
               The start of the region to transform FROM
  Arg [3]    : int $frm_end
               The end of the region to transform FROM
  Arg [4]    : int $strand
               The strand of the region to transform FROM
  Arg [5]    : Bio::EnsEMBL::CoordSystem
               The coordinate system to transform FROM
  Arg [6]    : (optional) fastmap
  Arg [7]    : (optional) Bio::Ensembl::Slice
               The slice to transform TO
  Example    : @coords = $asm_mapper->map('X', 1_000_000, 2_000_000,
                                            1, $chr_cs);
  Description: Transforms coordinates from one coordinate system
               to another.
  Returntype : List of Bio::EnsEMBL::Mapper::Coordinate and/or
               Bio::EnsEMBL::Mapper:Gap objects
  Exceptions : thrown if the specified TO coordinat system is not one
               of the coordinate systems associated with this assembly mapper
  Caller     : general
  Status     : Stable

=cut
*/

MapperRangeSet *ChainedAssemblyMapper_map(ChainedAssemblyMapper *cam, char *frmSeqRegionName, long frmStart, long frmEnd, int frmStrand, 
                              CoordSystem *frmCs, int fastmap, Slice *toSlice) {

  Mapper *mapper       = ChainedAssemblyMapper_getFirstLastMapper(cam);
  CoordSystem *firstCs = ChainedAssemblyMapper_getFirstCoordSystem(cam);
  CoordSystem *lastCs  = ChainedAssemblyMapper_getLastCoordSystem(cam);

  int isInsert = (frmStart == frmEnd+1);

  char *frm;
  RangeRegistry *registry;

  Vector *tmp = Vector_new();
  Vector_addElement(tmp, frmSeqRegionName);

  AssemblyMapperAdaptor *adaptor = ChainedAssemblyMapper_getAdaptor(cam);

  Vector *idVec = AssemblyMapperAdaptor_seqRegionsToIds(frmCs, tmp);

  IDType seqRegionId = *(Vector_getElementAt(idVec, 0));

  Vector_free(tmp, NULL);
  Vector_free(idVec, NULL);

  // speed critical section:
  // try to do simple pointer equality comparisons of the coord system objects
  // first since this is likely to work most of the time and is much faster
  // than a function call

  if (frmCs == firstCs || (frmCs != lastCs && !CoordSystem_compare(frmCs, firstCs))) {
    frm = "first";
    registry = ChainedAssemblyMapper_getFirstRegistry(cam);
  } else if (frmCs == lastCs || !CoordSystem_compare(frmCs, lastCs)) {
    frm = "last";
    registry = ChainedAssemblyMapper_getLastRegistry(cam);
  } else {
    fprintf(stderr,"Coordinate system %s %s is neither the first nor the last coordinate system "
                   " of this ChainedAssemblyMapper\n", CoordSystem_getName(frmCs), CoordSystem_getVersion(frmCs) );
    exit(1);
  }

  // the minimum area we want to register if registration is necessary is
  // about 1MB. Break requested ranges into chunks of 1MB and then register
  // this larger region if we have a registry miss.

  // use bitwise shift for fast and easy integer multiplication and division
  long minStart, minEnd;

  if (isInsert) {
    minStart = ((frmEnd >> CHUNKFACTOR) << CHUNKFACTOR);
    minEnd   = (((frmStart >> CHUNKFACTOR) + 1) << CHUNKFACTOR) - 1 ;
  } else {
    minStart = ((frmStart >> CHUNKFACTOR) << CHUNKFACTOR);
    minEnd   = (((frmEnd >> CHUNKFACTOR) + 1) << CHUNKFACTOR) - 1 ;
  }

  // get a list of ranges in the requested region that have not been registered,
  // and register them at the same

  Vector *ranges;

  if (isInsert) {
    ranges = RangeRegistry_checkAndRegister(registry, seqRegionId, frmEnd, frmStart, minStart, minEnd);
  } else {
    ranges = RangeRegistry_checkAndRegister(registry, seqRegionId, frmStart, frmEnd, minStart, minEnd);
  }

  if (Vector_getNumElement(ranges)) {
    if (ChainedAssemblyMapper_getSize() > ChainedAssemblyMapper_getMaxPairCount(cam)) {
      ChainedAssemblyMapper_flush(cam);

      if (isInsert) {
        ranges = RangeRegistry_checkAndRegister(registry, seqRegionId, frmEnd, frmStart, minStart, minEnd);
      } else {
        ranges = RangeRegistry_checkAndRegister(registry, seqRegionId, frmStart, frmEnd, minStart, minEnd);
      }
    }
    AssemblyMapperAdaptor *adaptor = ChainedAssemblyMapper_getAdaptor(cam);
    AssemblyMapperAdaptor_registerChained(adaptor, cam, frm, seqRegionId, ranges, toSlice);
  }

  MapperRangeSet *mrs;
  if (fastmap) {
    mrs = Mapper_fastMap(mapper, seqRegionId, frmStart, frmEnd, frmStrand, frm);
  } else {
    mrs = Mapper_mapCoordinates(mapper, seqRegionId, frmStart, frmEnd, frmStrand, frm);
  }

  // Need to tidy up
  Vector_free(ranges, CoordPair_free);

  return mrs;
}


MapperRangeSet *ChainedAssemblyMapper_fastMap(ChainedAssemblyMapper *cam, char *frmSeqRegionName, long frmStart, long frmEnd, int frmStrand, 
                              CoordSystem *frmCs) {
  return ChainedAssemblyMapper_map(cam, frmSeqRegionName, frmStart, frmEnd, frmStrand, frmCs, 1, NULL);
}


/*
=head2 list_ids

  Arg [1]    : string $frm_seq_region
               The name of the sequence region of interest
  Arg [2]    : int $frm_start
               The start of the region of interest
  Arg [3]    : int $frm_end
               The end of the region to transform of interest
  Arg [5]    : Bio::EnsEMBL::CoordSystem $frm_cs
               The coordinate system to obtain overlapping ids of
  Example    : foreach $id ($asm_mapper->list_ids('X',1,1000,$chr_cs)) {...}
  Description: Retrieves a list of overlapping seq_region internal identifiers
               of another coordinate system.  This is the same as the
               list_seq_regions method but uses internal identfiers rather
               than seq_region strings
  Returntype : List of ints
  Exceptions : none
  Caller     : general
  Status     : Stable

=cut
*/


Vector *ChainedAssemblyMapper_listIds(ChainedAssemblyMapper *cam, char *fromSeqRegionName, long frmStart, long frmEnd, CoordSystem *frmCs) {

  int isInsert = (frmStart == frmEnd+1);

  //the minimum area we want to register if registration is necessary is
  //about 1MB. Break requested ranges into chunks of 1MB and then register
  //this larger region if we have a registry miss.

  //use bitwise shift for fast and easy integer multiplication and division
  long minStart, minEnd;

  if (isInsert) {
    minStart = ((frmEnd >> CHUNKFACTOR) << CHUNKFACTOR);
    minEnd   = (((frmStart >> CHUNKFACTOR) + 1) << CHUNKFACTOR) - 1;
  } else {
    minStart = ((frmStart >> CHUNKFACTOR) << CHUNKFACTOR);
    minEnd   = (((frmEnd >> CHUNKFACTOR) + 1) << CHUNKFACTOR) - 1;
  }

  // Not the most efficient thing to do making these temporary vectors to get one value, but hey its what the perl does!
  Vector *tmp = Vector_new();
  Vector_addElement(tmp, frmSeqRegionName);

  AssemblyMapperAdaptor *adaptor = ChainedAssemblyMapper_getAdaptor(cam);

  Vector *idVec = AssemblyMapperAdaptor_seqRegionsToIds(frmCs, tmp);

  IDType seqRegionId = *(Vector_getElementAt(idVec, 0));

  Vector_free(tmp, NULL);
  Vector_free(idVec, NULL);
  // End of somewhat inefficient stuff


  if (!CoordSystem_compare(frmCs, ChainedAssemblyMapper_getFirstCoordSystem(cam))) {
    RangeRegistry *registry = ChainedAssemblyMapper_getFirstRegistry(cam);

    Vector *ranges;

    if (isInsert) {
      ranges = RangeRegistry_checkAndRegister(registry, seqRegionId, frmEnd, frmStart, minStart, minEnd);
    } else {
      ranges = RangeRegistry_checkAndRegister(registry, seqRegionId, frmStart, frmEnd, minStart, minEnd);
    }

    if (defined($ranges)) {
      $self->adaptor->register_chained($self,"first",$seq_region_id,$ranges);
    }

    MapperPairSet *mps = Mapepr_listPairs( ChainedAssemblyMapper_getFirstLastMapper(cam), seqRegionId, frmStart, frmEnd, "first");

    return MapperPairSet_getToIds(mps) ;

  } else if (!CoordSystem_compare(frmCs, ChainedAssemblyMapper_getLastCoordSystem(cam))) {
    RangeRegistry *registry = ChainedAssemblyMapper_getLastRegistry(cam);

    Vector *ranges;

    if (isInsert) {
      ranges = RangeRegistry_checkAndRegister(registry, seqRegionId, frmEnd, frmStart, minStart, minEnd);
    } else {
      ranges = RangeRegistry_checkAndRegister(registry, seqRegionId, frmStart, frmEnd, minStart, minEnd);
    }

    if (Vector_getNumElement(ranges)) {
      AssemblyMapperAdaptor *adaptor = ChainedAssemblyMapper_getAdaptor(cam);
      AssemblyMapperAdaptor_registerChained(adaptor, cam, "last", seqRegionId, ranges);
    }

    MapperPairSet *mps = Mapepr_listPairs( ChainedAssemblyMapper_getFirstLastMapper(cam), seqRegionId, frmStart, frmEnd, "last");

    return MapperPairSet_getFromIds(mps) ;
  } else {
    fprintf(stderr,"Coordinate system %s %s is neither the first nor the last coordinate system "
                    " of this ChainedAssemblyMapper\n", CoordSystem_getName(frmCs), CoordSystem_getVersion(frmCs) );
    exit(1);
  }
}


/*
=head2 list_seq_regions

  Arg [1]    : string $frm_seq_region
               The name of the sequence region of interest
  Arg [2]    : int $frm_start
               The start of the region of interest
  Arg [3]    : int $frm_end
               The end of the region to transform of interest
  Arg [5]    : Bio::EnsEMBL::CoordSystem $frm_cs
               The coordinate system to obtain overlapping ids of
  Example    : foreach $id ($asm_mapper->list_ids('X',1,1000,$ctg_cs)) {...}
  Description: Retrieves a list of overlapping seq_region internal identifiers
               of another coordinate system.  This is the same as the
               list_ids method but uses seq_region names rather internal ids
  Returntype : List of strings
  Exceptions : none
  Caller     : general
  Status     : Stable

=cut
*/

Vector *ChainedAssemblyMapper_listSeqRegions(ChainedAssemblyMapper *cam, char *fromSeqRegionName, long frmStart, long frmEnd, CoordSystem *frmCs) {

  //retrieve the seq_region names
  Vector *seqRegs = ChainedAssemblyMapper_listIds(frmSeqRegionName, frmStart, frmEnd, frmCs);

  // The seq_regions are from the 'to' coordinate system not the
  // from coordinate system we used to obtain them
// SMJS toCs doesn't seem to be used
//  CoordSystem *toCs;
//  if (!CoordSystem_compare(frmCs, ChainedAssemblyMapper_getFirstCoordSystem(cam))) {
//    toCs = ChainedAssemblyMapper_getLastCoordSystem(cam);
//  } else {
//    toCs = ChainedAssemblyMapper_getFirstCoordSystem(cam);
//  }

  // convert them to names
  AssemblyMapperAdaptor *adaptor = ChainedAssemblyMapper_getAdaptor(cam);

  Vector *regions = AssemblyMapperAdaptor_seqIdsToRegions(adaptor, seqRegs);
  // Need to tidy up seqRegs;
  Vector_free(seqRegs, NULL);

  return regions;
}

/*
=head2 first_last_mapper

  Args       : none
  Example    : $mapper = $cam->first_last_mapper();
  Description: return the mapper.
  Returntype : Bio::EnsEMBL::Mapper
  Exceptions : none
  Caller     : internal
  Status     : Stable

=cut
*/

sub first_last_mapper {
  my $self = shift;
  return $self->{'first_last_mapper'};
}

/*
=head2 first_middle_mapper

  Args       : none
  Example    : $mapper = $cam->first_middle_mapper();
  Description: return the mapper.
  Returntype : Bio::EnsEMBL::Mapper
  Exceptions : none
  Caller     : internal
  Status     : Stable

=cut
*/


sub first_middle_mapper {
  my $self = shift;
  return $self->{'first_mid_mapper'};
}

/*
=head2 last_middle_mapper

  Args       : none
  Example    : $mapper = $cam->last_middle_mapper();
  Description: return the mapper.
  Returntype : Bio::EnsEMBL::Mapper
  Exceptions : none
  Caller     : internal
  Status     : Stable

=cut
*/

sub last_middle_mapper {
  my $self = shift;
  return $self->{'last_mid_mapper'};
}


/*
=head2 first_CoordSystem

  Args       : none
  Example    : $coordsys = $cam->first_CoordSystem();
  Description: return the CoordSystem.
  Returntype : Bio::EnsEMBL::CoordSystem
  Exceptions : none
  Caller     : internal
  Status     : Stable

=cut
*/

sub first_CoordSystem {
  my $self = shift;
  return $self->{'first_cs'};
}

/*
=head2 middle_CoordSystem

  Args       : none
  Example    : $coordsys = $cam->middle_CoordSystem();
  Description: return the CoordSystem.
  Returntype : Bio::EnsEMBL::CoordSystem
  Exceptions : none
  Caller     : internal
  Status     : Stable

=cut
*/

sub middle_CoordSystem {
  my $self = shift;
  return $self->{'mid_cs'};
}

/*
=head2 last_CoordSystem

  Args       : none
  Example    : $coordsys = $cam->last_CoordSystem();
  Description: return the CoordSystem.
  Returntype : Bio::EnsEMBL::CoordSystem
  Exceptions : none
  Caller     : internal
  Status     : Stable

=cut
*/

sub last_CoordSystem {
  my $self = shift;
  return $self->{'last_cs'};
}

/*
=head2 first_registry

  Args       : none
  Example    : $rr = $cam->first_registry();
  Description: return the Registry.
  Returntype : Bio::EnsEMBL::Mapper::RangeRegistry
  Exceptions : none
  Caller     : internal
  Status     : Stable

=cut
*/

sub first_registry {
  my $self = shift;
  return $self->{'first_registry'};
}

/*
=head2 last_registry

  Args       : none
  Example    : $rr = $cam->last_registry();
  Description: return the Registry.
  Returntype : Bio::EnsEMBL::Mapper::RangeRegistry
  Exceptions : none
  Caller     : internal
  Status     : Stable

=cut
*/

sub last_registry {
  my $self = shift;
  return $self->{'last_registry'};
}


/*
 Methods supplied to maintain polymorphism with AssemblyMapper there
 is no real assembled or component in the chained mapper, since the
 ordering is arbitrary and both ends might actually be assembled, but
 these methods provide convenient synonyms
*/

/*
=head2 mapper

  Args       : none
  Example    : $mapper = $cam->mapper();
  Description: return the first_last_mapper.
  Returntype : Bio::EnsEMBL::Mapper
  Exceptions : none
  Caller     : internal
  Status     : Stable

=cut
*/

sub mapper {
  my $self = shift;
  return $self->first_last_mapper();
}

/*
=head2 assembled_CoordSystem

  Args       : none
  Example    : $coordsys = $cam->assembled_CoordSystem();
  Description: return the first CoordSystem.
  Returntype : Bio::EnsEMBL::CoordSystem
  Exceptions : none
  Caller     : internal
  Status     : Stable

=cut
*/

sub assembled_CoordSystem {
  my $self = shift;
  return $self->{'first_cs'};
}

/*
=head2 component_CoordSystem

  Args       : none
  Example    : $coordsys = $cam->component_CoordSystem();
  Description: return the last CoordSystem.
  Returntype : Bio::EnsEMBL::CoordSystem
  Exceptions : none
  Caller     : internal
  Status     : Stable

=cut
*/

sub component_CoordSystem {
  my $self = shift;
  return $self->{'last_cs'};
}


/*
=head2 adaptor

  Arg [1]    : Bio::EnsEMBL::DBSQL::AssemblyMapperAdaptor $adaptor
  Description: get/set for this objects database adaptor
  Returntype : Bio::EnsEMBL::DBSQL::AssemblyMapperAdaptor
  Exceptions : none
  Caller     : general
  Status     : Stable

=cut
*/

sub adaptor {
  my $self = shift;
  weaken($self->{'adaptor'} = shift) if(@_);
  return $self->{'adaptor'};
}


/* Hopefully don't need these deprecated methods

=head2 in_assembly

  Deprecated. Use map() or list_ids() instead

=cut

sub in_assembly {
  my ($self, $object) = @_;

  deprecate('Use map() or list_ids() instead.');

  my $csa = $self->db->get_CoordSystemAdaptor();

  my $top_level = $csa->fetch_top_level();

  my $asma = $self->adaptor->fetch_by_CoordSystems($object->coord_system(),
                                                   $top_level);

  my @list = $asma->list_ids($object->seq_region(), $object->start(),
                             $object->end(), $object->coord_system());

  return (@list > 0);
}


=head2 map_coordinates_to_assembly

  DEPRECATED use map() instead

=cut

sub map_coordinates_to_assembly {
  my ($self, $contig_id, $start, $end, $strand) = @_;

  deprecate('Use map() instead.');

  #not sure if contig_id is seq_region_id or name...
  return $self->map($contig_id, $start, $end, $strand,
                   $self->contig_CoordSystem());

}


=head2 fast_to_assembly

  DEPRECATED use map() instead

=cut

sub fast_to_assembly {
  my ($self, $contig_id, $start, $end, $strand) = @_;

  deprecate('Use map() instead.');

  #not sure if contig_id is seq_region_id or name...
  return $self->map($contig_id, $start, $end, $strand,
                    $self->contig_CoordSystem());
}


=head2 map_coordinates_to_rawcontig

  DEPRECATED use map() instead

=cut

sub map_coordinates_to_rawcontig {
  my ($self, $chr_name, $start, $end, $strand) = @_;

  deprecate('Use map() instead.');

  return $self->map($chr_name, $start, $end, $strand,
                    $self->assembled_CoordSystem());

}

=head2 list_contig_ids
  DEPRECATED Use list_ids instead

=cut

sub list_contig_ids {
  my ($self, $chr_name, $start, $end) = @_;

  deprecate('Use list_ids() instead.');

  return $self->list_ids($chr_name, $start, $end,
                         $self->assembled_CoordSystem());
}
*/
