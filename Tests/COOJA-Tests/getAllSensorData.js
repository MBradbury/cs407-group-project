GENERATE_MSG(60000, "sleep"); //Wait for 60 secs, give the nodes a chance to start up
YIELD_THEN_WAIT_UNTIL(msg.equals("sleep"));

printAllIDs(); //Print all the IDs of the nodes

//prints the information of the node
goThroughNodes(function(m) {
	log.log("\nMote ID = " + m.getID() + "\n");
	log.log("Rime Address = " + m.getInterfaces().getRimeAddress().getAddressString() + "\n");
	log.log("Temprature =  " + m.getInterfaces().get("Temperature").getTemperature() +"\n");
	log.log("Humidity = " + m.getInterfaces().get("Humidity").getHumidity() + "\n");
	printRimeStats(m);
});

log.log("\n");
log.testOK(); /* Report test success and quit */

//Prints the stats stored in the rimestats variable
function printRimeStats(m)
{
	//if the option has been enabled
	if (m.getMemory().variableExists("rimestats")) {
		var ar = m.getMemory().getMemorySegment(m.getMemory().getVariableAddress("rimestats"), 72);

		for (var i = 0; i < ar.length; i = i + 4) {
			switch (i / 4) {
				case  0: log.log("tx"); break;
				case  1: log.log("rx"); break;
				case  2: log.log("reliabletx"); break; 
				case  3: log.log("reliablerx"); break;
				case  4: log.log("rexmit"); break; 
				case  5: log.log("acktx"); break;
				case  6: log.log("noacktx"); break; 
				case  7: log.log("ackrx"); break;
				case  8: log.log("timedout"); break; 
				case  9: log.log("badackrx"); break;
				case 10: log.log("toolong"); break; 
				case 11: log.log("tooshort"); break; 
				case 12: log.log("badsynch"); break; 
				case 13: log.log("badcrc"); break;
				case 14: log.log("contentiondrop"); break;
				case 15: log.log("sendingdrop"); break;
				case 16: log.log("lltx"); break;
				case 17: log.log("llrx"); break;
			}
			log.log(" = " + byteArrayToLong(ar.slice(i,i+3)) + "\n");
		}
	}
}

function byteArrayToLong(/*byte[]*/byteArray) {
    var value = 0;
    for ( var i = byteArray.length - 1; i >= 0; i--) {
        value = (value * 256) + byteArray[i];
    }

    return value;
}

function goThroughNodes(func) {
	allmotes = sim.getMotes();

	for(var i = 0; i < allmotes.length; i++) {
		func(allmotes[i]);
	}
}

function printAllIDs() {
	log.log("Mote IDs: [");

	goThroughNodes(function(mote) {
		log.log(" " + mote.getID());
	});
	log.log(" ]\n");
}