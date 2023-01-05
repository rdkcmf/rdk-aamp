marker_tsprocessor = [{
		"pattern":"Discard ES Type %% position ",
		"label":"Discard ES"
},
	{
		"pattern":"Send : pts %% dts %%",
		"label":"Send PTS(%0) DTS(%1)"
},
	{
		"pattern":"resetBasePTS",
		"label":"Reset base PTS"
},
	{
		"pattern":"Type[%%], basePTS %% final %%",
		"label":"Set base PTS"
},
	{
		"pattern":"PTS Rollover",
		"label":"PTS Rollover"
},
	{
		"pattern":"PTS updated",
		"label":"PTS updated"
},
	{
		"pattern":"base_pts not initialized",
		"label":"Base pts not init"
},
	{
		"pattern":"current_pts[%%] < base_pts[%%],",
		"label":"Current pts < Base pts"
},
	{
		"pattern":"delta[%%] > MAX_FIRST_PTS_OFFSET,",
		"label":"Pts delta > max first pts offset"
},
	{
		"pattern":"PTS in range.delta[%%] <= MAX_FIRST_PTS_OFFSET",
		"label":"Pts delta <= max first pts offset"
},
	{
		"pattern":"base_pts not available",
		"label":"Base PTS NA"
},
	{
		"pattern":"base_pts update from %% to %%",
		"label":"Base PTS updated"
},
	{
		"pattern":"PTS NOT present pesStart",
		"label":"No PTS in PES"
},
	{
		"pattern":"DTS NOT present pesStart",
		"label":"No DTS in PES"
},
	{
		"pattern":"Optional pes header NOT present pesStart",
		"label":"Opt PES header NA"
},
	{
		"pattern":"Packet start prefix check failed",
		"label":"Packet check failed"
},
	{
		"pattern":"Avoiding PTS check when new audio or video TS packet is received without proper PES data",
		"label":"Avoid PTS check"
},
	{
		"pattern":"PES_STATE_WAITING_FOR_HEADER , discard data. ",
		"label":"Discard data"
},
	{
		"pattern":"Packet start prefix check failed",
		"label":"Packet chck failed"
},
	{
		"pattern":"Optional header not preset pesStart",
		"label":"Opt header in PES"
},
	{
		"pattern":"Invalid pes_state.",
		"label":"Invalid PES"
},
	{
		"pattern":"No payload in packet packetStart",
		"label":"No payload in Packet"
},
	{
		"pattern":"constructor -",
		"label":"TSProcessor::Construct"
},
	{
		"pattern":"destructor -",
		"label":"TSProcessor::Destructor"
},
	{
		"pattern":"Warning: RecordContext: pmt contains more than %% video PIDs",
		"label":"More Video pid found"
},
	{
		"pattern":"Warning: RecordContext: pmt contains more than %% audio PIDs",
		"label":"More Audio pid found"
},
	{
		"pattern":"Replace PAT/PMT",
		"label":"Replace PAT/PMT"
},
	{
		"pattern":"Remove PAT/PMT",
		"label":"Remove PAT/PMT"
},
	{
		"pattern":"Error: data buffer not TS packet aligned",
		"label":"Buffer not aligned"
},
	{
		"pattern":"input SPTS discontinuity on pid",
		"label":"SPTS discontinuity"
},
	{
		"pattern":"RecordContext: PAT is MPTS",
		"label":"PAT is MPTS"
},
	{
		"pattern":"RecordContext: pmt change detected",
		"label":"PMT change detected"
},
	{
		"pattern":"Warning: RecordContext: ignoring pid 0 ",
		"label":"Ignoring pid 0"
},
	{
		"pattern":"Error: unable to allocate pmt collector buffer",
		"label":"Unable to alloc buffer"
},
	{
		"pattern":"Warning: RecordContext: ignoring pmt",
		"label":"Ignore PMT"
},
	{
		"pattern":"ignoring unexpected pmt TS",
		"label":"Ignore PMT TS"
},
	{
		"pattern":"RecordContext: pts discontinuity",
		"label":"PTS Discontinuity"
},
	{
		"pattern":"discontinuous buffer- flushing video demux",
		"label":"Disco buffer, flush video"
},
	{
		"pattern":"discontinuous buffer- flushing audio demux",
		"label":"Disco buffer, flush audio"
},
	{
		"pattern":"PCR not available before ES packet",
		"label":"No PCR"
},
	{
		"pattern":"Using first video pts as base pts",
		"label":"Fiurst Video pts as base pts"
},
	{
		"pattern":"Using first audio pts as base pts",
		"label":"Fiurst Audio pts as base pts"
},
	{
		"pattern":"PTS error, discarding segment",
		"label":"PTS Error, discard Seg "
},
	{
		"pattern":"Segment doesn't starts with valid TS packet, discarding. Dump first packet",
		"label":"Discard TS packet"
},
	{
		"pattern":"Discarding %% bytes at end",
		"label":"Discard bytes at end"
},
	{
		"pattern":"wait for base PTS",
		"label":"Wait for Base PTS"
},
	{
		"pattern":"got base PTS.",
		"label":"Got Base PTS"
},
	{
		"pattern":"It is possibly MC Channel",
		"label":"Possibily Music Channel"
}];
