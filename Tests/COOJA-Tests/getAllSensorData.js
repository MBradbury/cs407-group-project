//import Java Package to JavaScript
importPackage(java.io);

GENERATE_MSG(10000, "sleep"); //Wait for 60 secs, give the nodes a chance to start up
YIELD_THEN_WAIT_UNTIL(msg.equals("sleep"));

TIMEOUT(1000000);

//file for rimestats
rimestats = new FileWriter("log_rimestats.txt", false);

// Use JavaScript object as an associative array
outputs = [];

file = new FileWriter("log_data.txt", false);

while (true) {
	if (msg.contains("Pred:")) {
	   	log.log(time + " " + id.toString() + " " + msg + "\n");

	   	round = new Object();
	   	
	   	round["data"] = [];

	   	if (msg.toLowerCase().contains("true"))
	   	{
	   		round["pred"] = "true";
	   	}
	   	else
	   	{
	   		round["pred"] = "false";
	   	}

	   	//get all the data from the nodes and place into objects
	   	goThroughNodes(function(m) {
	   		temp = "" + m.getInterfaces().get("Temperature").getTemperature();
	   		hum = "" + m.getInterfaces().get("Humidity").getHumidity();

	   		json = {
	   			"id":m.getID().toString(),
	   			"temp":temp.toString(),
	   			"Humidity":hum.toString(),
	   		};

	   		round["data"].push(json);
	   	});

	   	outputs.push(round);

		//close and open the file to overwrite what was there previously
		file.close();
		file = new FileWriter("log_data.txt", false);
	   	file.write(JSON.stringify(outputs) + "\n");
	 	
	 	//Save all the rimestats
	 	rimes = [];
	 	allmotes = sim.getMotes();
		for(var i = 0; i < allmotes.length; i++) {
			rimes.push(getRimeStats(allmotes[i]));
		}

		//close and open the file, to overwrite what was there previously
		rimestats.close();
		rimestats = new FileWriter("log_rimestats.txt", false);
		rimestats.write(JSON.stringify(rimes) + "\n");
	}
 	try{
 		//This is the tricky part. The Script is terminated using
 		// an exception. This needs to be caught.
 		YIELD();
 	} catch (e) {
 		//Close files.
 		file.close();
		rimestats.close();
 		//Rethrow exception again, to end the script.
 		throw('test script killed');
	}
}

//Prints the stats stored in the rimestats variable
function getRimeStats(m)
{
	out = new Object();
	out["id"] = m.getID().toString(); //ID of the node

	//if the option has been enabled
	if (m.getMemory().variableExists("rimestats")) {
		var ar = m.getMemory().getMemorySegment(m.getMemory().getVariableAddress("rimestats"), 72);

		for (var i = 0; i < ar.length; i = i + 4) {
			switch (i / 4) {
				case  0: out["tx"] = byteArrayToLong(ar.slice(i, i+3)); break;
				case  1: out["rx"] = byteArrayToLong(ar.slice(i, i+3)); break;
				case  2: out["reliabletx"] = byteArrayToLong(ar.slice(i, i+3)); break; 
				case  3: out["reliablerx"] = byteArrayToLong(ar.slice(i, i+3)); break;
				case  4: out["rexmit"] = byteArrayToLong(ar.slice(i, i+3)); break; 
				case  5: out["acktx"] = byteArrayToLong(ar.slice(i, i+3)); break;
				case  6: out["noacktx"] = byteArrayToLong(ar.slice(i, i+3)); break; 
				case  7: out["ackrx"] = byteArrayToLong(ar.slice(i, i+3)); break;
				case  8: out["timedout"] = byteArrayToLong(ar.slice(i, i+3)); break; 
				case  9: out["badackrx"] = byteArrayToLong(ar.slice(i, i+3)); break;
				case 10: out["toolong"] = byteArrayToLong(ar.slice(i, i+3)); break; 
				case 11: out["tooshort"] = byteArrayToLong(ar.slice(i, i+3)); break; 
				case 12: out["badsynch"] = byteArrayToLong(ar.slice(i, i+3)); break; 
				case 13: out["badcrc"] = byteArrayToLong(ar.slice(i, i+3)); break;
				case 14: out["contentiondrop"] = byteArrayToLong(ar.slice(i, i+3)); break;
				case 15: out["sendingdrop"] = byteArrayToLong(ar.slice(i, i+3)); break;
				case 16: out["lltx"] = byteArrayToLong(ar.slice(i, i+3)); break;
				case 17: out["llrx"] = byteArrayToLong(ar.slice(i, i+3)); break;
			}
		}
	}
	return out;
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