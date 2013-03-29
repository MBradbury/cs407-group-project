//import Java Package to JavaScript
importPackage(java.io);

// Define a startsWith function for convenience
if (typeof String.prototype.startsWith != 'function')
{
	String.prototype.startsWith = function (str) {
		return this.slice(0, str.length) == str;
	};
}

// We need to fix strings for some reason
// http://www.mirthcorp.com/community/forums/showthread.php?t=5128
// http://nelsonwells.net/2012/02/json-stringify-with-mapped-variables/
function sf(s) {
	return new String(s);
}

// Use JavaScript object as an associative array
var results = new Object();
results["stats"] = new Object();
results["stats"]["rime"] = [];
results["stats"]["energy"] = [];
results["predicateFailures"] = [];
results["motes"] = [];

var allmotes = sim.getMotes();
for (var i = 0; i < allmotes.length; ++i)
{
	results["motes"].push(allmotes[i].getID());
}


// Set a timeout of 920 seconds
// the units this function takes is in milliseconds
TIMEOUT(920000);

GENERATE_MSG(900000, "END"); //Wait for 900 secs

while (true)
{
	YIELD();

	if (msg.equals("END"))
	{
		break;
	}
	else if (msg.startsWith("PF "))
	{
		var pf = new Object();

		var splitMsg = msg.substring(msg.indexOf("*") + 1, msg.lastIndexOf("*")).split(":");

		pf["on"] = sf(id);
		pf["node"] = sf(splitMsg[0]);
		pf["predicateId"] = parseInt(splitMsg[1]);

		pf["variableDetails"] = [];

		var detailsSplit = splitMsg[2].split(",");
		for (var i = 0; i < detailsSplit.length; ++i)
		{
			var detailSplit = detailsSplit[i].split("#");

			var details = new Object();
			details["hops"] = parseInt(detailSplit[0]);
			details["variableId"] = parseInt(detailSplit[1]);
			details["length"] = parseInt(detailSplit[2]);

			pf["variableDetails"].push(details);
		}


		pf["values"] = [];

		var valuesSplit = splitMsg[3].split("\\|");
		for (var i = 0; i < valuesSplit.length; ++i)
		{
			var nodeData = new Object();

			var valueSplit = valuesSplit[i].split(",");

			for (var j = 0; j < valueSplit.length; ++j)
			{
				var keyVals = valueSplit[j].split("=");

				nodeData[parseInt(keyVals[0])] =  sf(keyVals[1]);
			}

			pf["values"].push(nodeData);
		}

		results["predicateFailures"].push(pf);
	}
	else if (msg.startsWith("E "))
	{
		var splitMsg = msg.split(" ");

		var data = new Object();

		for (var i = 0; i < splitMsg.length; i += 2)
		{
			data[splitMsg[i]] = parseInt(splitMsg[i + 1]);
		}

		results["stats"]["energy"].push(data);

	}
	else if (msg.startsWith("S "))
	{
		var splitMsg = msg.split(" ");

		var data = new Object();

		for (var i = 0; i < splitMsg.length; i += 2)
		{
			data[splitMsg[i]] = parseInt(splitMsg[i + 1]);
		}

		results["stats"]["rime"].push(data);
	}
	else if (msg.contains("END"))
	{
		log.log(JSON.stringify(results));
	}
}

// Write out our results
log.log(JSON.stringify(results) + "\n");

log.testOK();
