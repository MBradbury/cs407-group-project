<?xml version="1.0" encoding="UTF-8"?>
<simconf>
  <project EXPORT="discard">[APPS_DIR]/mrm</project>
  <project EXPORT="discard">[APPS_DIR]/mspsim</project>
  <project EXPORT="discard">[APPS_DIR]/avrora</project>
  <project EXPORT="discard">[APPS_DIR]/serial_socket</project>
  <project EXPORT="discard">[APPS_DIR]/collect-view</project>
  <project EXPORT="discard">[APPS_DIR]/powertracker</project>
  <simulation>
    <title>TDMA</title>
    <randomseed>generated</randomseed>
    <motedelay_us>1000000</motedelay_us>
    <radiomedium>
      se.sics.cooja.radiomediums.UDGM
      <transmitting_range>50.0</transmitting_range>
      <interference_range>100.0</interference_range>
      <success_ratio_tx>1.0</success_ratio_tx>
      <success_ratio_rx>1.0</success_ratio_rx>
    </radiomedium>
    <events>
      <logoutput>40000</logoutput>
    </events>
    <motetype>
      se.sics.cooja.mspmote.SkyMoteType
      <identifier>sky1</identifier>
      <description>TDMA</description>
      <source EXPORT="discard">/home/user/cs407-group-project/Algorithms/TDMA/tdma.c</source>
      <commands EXPORT="discard">make tdma.sky TARGET=sky</commands>
      <firmware EXPORT="copy">/home/user/cs407-group-project/Algorithms/TDMA/tdma.sky</firmware>
      <moteinterface>se.sics.cooja.interfaces.Position</moteinterface>
      <moteinterface>se.sics.cooja.interfaces.RimeAddress</moteinterface>
      <moteinterface>se.sics.cooja.interfaces.IPAddress</moteinterface>
      <moteinterface>se.sics.cooja.interfaces.Mote2MoteRelations</moteinterface>
      <moteinterface>se.sics.cooja.interfaces.MoteAttributes</moteinterface>
      <moteinterface>se.sics.cooja.mspmote.interfaces.MspClock</moteinterface>
      <moteinterface>se.sics.cooja.mspmote.interfaces.MspMoteID</moteinterface>
      <moteinterface>se.sics.cooja.mspmote.interfaces.SkyButton</moteinterface>
      <moteinterface>se.sics.cooja.mspmote.interfaces.SkyFlash</moteinterface>
      <moteinterface>se.sics.cooja.mspmote.interfaces.Msp802154Radio</moteinterface>
      <moteinterface>se.sics.cooja.mspmote.interfaces.MspSerial</moteinterface>
      <moteinterface>se.sics.cooja.mspmote.interfaces.SkyLED</moteinterface>
      <moteinterface>se.sics.cooja.mspmote.interfaces.MspDebugOutput</moteinterface>
      <moteinterface>se.sics.cooja.mspmote.interfaces.SkyTemperature</moteinterface>
      <moteinterface>se.sics.cooja.mspmote.interfaces.SkyHumidity</moteinterface>
    </motetype>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>0.0</x>
        <y>0.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>1</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>0.0</x>
        <y>40.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>2</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>0.0</x>
        <y>80.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>3</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>0.0</x>
        <y>120.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>4</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>0.0</x>
        <y>160.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>5</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>40.0</x>
        <y>0.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>6</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>40.0</x>
        <y>40.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>7</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>40.0</x>
        <y>80.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>8</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>40.0</x>
        <y>120.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>9</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>40.0</x>
        <y>160.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>10</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>80.0</x>
        <y>0.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>11</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>80.0</x>
        <y>40.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>12</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>80.0</x>
        <y>80.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>13</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>80.0</x>
        <y>120.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>14</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>80.0</x>
        <y>160.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>15</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>120.0</x>
        <y>0.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>16</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>120.0</x>
        <y>40.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>17</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>120.0</x>
        <y>80.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>18</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>120.0</x>
        <y>120.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>19</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>120.0</x>
        <y>160.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>20</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>160.0</x>
        <y>0.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>21</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>160.0</x>
        <y>40.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>22</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>160.0</x>
        <y>80.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>23</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>160.0</x>
        <y>120.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>24</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>160.0</x>
        <y>160.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>25</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>200.0</x>
        <y>0.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>26</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>200.0</x>
        <y>40.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>27</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>200.0</x>
        <y>80.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>28</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>200.0</x>
        <y>120.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>29</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
    <mote>
      <breakpoints />
      <interface_config>
        se.sics.cooja.interfaces.Position
        <x>200.0</x>
        <y>160.0</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        se.sics.cooja.mspmote.interfaces.MspMoteID
        <id>30</id>
      </interface_config>
      <motetype_identifier>sky1</motetype_identifier>
    </mote>
  </simulation>
  <plugin>
    se.sics.cooja.plugins.SimControl
    <width>280</width>
    <z>5</z>
    <height>160</height>
    <location_x>400</location_x>
    <location_y>0</location_y>
  </plugin>
  <plugin>
    se.sics.cooja.plugins.Visualizer
    <plugin_config>
      <skin>se.sics.cooja.plugins.skins.IDVisualizerSkin</skin>
      <viewport>1.7636363636363637 0.0 0.0 1.7636363636363637 17.636363636363637 31.909090909090914</viewport>
    </plugin_config>
    <width>400</width>
    <z>4</z>
    <height>400</height>
    <location_x>1</location_x>
    <location_y>1</location_y>
  </plugin>
  <plugin>
    se.sics.cooja.plugins.LogListener
    <plugin_config>
      <filter />
    </plugin_config>
    <width>424</width>
    <z>3</z>
    <height>240</height>
    <location_x>400</location_x>
    <location_y>160</location_y>
  </plugin>
  <plugin>
    se.sics.cooja.plugins.TimeLine
    <plugin_config>
      <mote>0</mote>
      <mote>1</mote>
      <mote>2</mote>
      <mote>3</mote>
      <mote>4</mote>
      <mote>5</mote>
      <mote>6</mote>
      <mote>7</mote>
      <mote>8</mote>
      <mote>9</mote>
      <mote>10</mote>
      <mote>11</mote>
      <mote>12</mote>
      <mote>13</mote>
      <mote>14</mote>
      <mote>15</mote>
      <mote>16</mote>
      <mote>17</mote>
      <mote>18</mote>
      <mote>19</mote>
      <mote>20</mote>
      <mote>21</mote>
      <mote>22</mote>
      <mote>23</mote>
      <mote>24</mote>
      <mote>25</mote>
      <mote>26</mote>
      <mote>27</mote>
      <mote>28</mote>
      <mote>29</mote>
      <showRadioRXTX />
      <showRadioHW />
      <showLEDs />
      <split>-1</split>
      <zoomfactor>500.0</zoomfactor>
    </plugin_config>
    <width>824</width>
    <z>2</z>
    <height>166</height>
    <location_x>0</location_x>
    <location_y>503</location_y>
  </plugin>
  <plugin>
    se.sics.cooja.plugins.Notes
    <plugin_config>
      <notes>Enter notes here</notes>
      <decorations>true</decorations>
    </plugin_config>
    <width>144</width>
    <z>1</z>
    <height>160</height>
    <location_x>680</location_x>
    <location_y>0</location_y>
  </plugin>
  <plugin>
    se.sics.cooja.plugins.ScriptRunner
    <plugin_config>
      <script>//import Java Package to JavaScript&#xD;
importPackage(java.io);&#xD;
importPackage(java.util.zip);&#xD;
&#xD;
// Define a startsWith function for convenience&#xD;
if (typeof String.prototype.startsWith != 'function')&#xD;
{&#xD;
	String.prototype.startsWith = function (str) {&#xD;
		return this.slice(0, str.length) == str;&#xD;
	};&#xD;
}&#xD;
&#xD;
outputDirectory = "/home/user/cs407-group-project/Results/TDMA/PELP/2HOP/30/4.0/"&#xD;
&#xD;
// We need a way to generate a 99% sure its unique filename&#xD;
function guid() {&#xD;
	return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {&#xD;
	    var r = Math.random()*16|0, v = c == 'x' ? r : (r&amp;0x3|0x8);&#xD;
	    return v.toString(16);&#xD;
	});&#xD;
}&#xD;
&#xD;
resultsFile = new BufferedWriter(new OutputStreamWriter(new GZIPOutputStream(new FileOutputStream(outputDirectory + guid()))));&#xD;
&#xD;
&#xD;
// We need to fix strings for some reason&#xD;
// http://www.mirthcorp.com/community/forums/showthread.php?t=5128&#xD;
// http://nelsonwells.net/2012/02/json-stringify-with-mapped-variables/&#xD;
function sf(s) {&#xD;
	return new String(s);&#xD;
}&#xD;
&#xD;
// Use JavaScript object as an associative array&#xD;
var results = new Object();&#xD;
results["stats"] = new Object();&#xD;
results["stats"]["rime"] = [];&#xD;
results["stats"]["energy"] = [];&#xD;
results["stats"]["TDMA"] = [];&#xD;
results["predicate"] = [];&#xD;
results["motes"] = [];&#xD;
&#xD;
var allmotes = sim.getMotes();&#xD;
for (var i = 0; i &lt; allmotes.length; ++i)&#xD;
{&#xD;
	results["motes"].push(allmotes[i].getID());&#xD;
}&#xD;
&#xD;
&#xD;
// Set a timeout of 35 minutes 20 seconds&#xD;
// the units this function takes is in milliseconds&#xD;
TIMEOUT(2120000);&#xD;
&#xD;
GENERATE_MSG(2100000, "END"); //Wait for 35 minutes&#xD;
&#xD;
while (true)&#xD;
{&#xD;
	YIELD();&#xD;
&#xD;
	if (msg.equals("END"))&#xD;
	{&#xD;
		break;&#xD;
	}&#xD;
	else if (msg.startsWith("StartPE"))&#xD;
	{&#xD;
		var splitMsg = msg.split(" ");&#xD;
&#xD;
		results["peType"] = sf(splitMsg[1]);&#xD;
		results["pePeriod"] = sf(splitMsg[2]);&#xD;
	}&#xD;
	else if (msg.startsWith("PF "))&#xD;
	{&#xD;
		var pf = new Object();&#xD;
&#xD;
		var splitMsg = msg.substring(msg.indexOf("*") + 1, msg.lastIndexOf("*")).split(":");&#xD;
&#xD;
		pf["on"] = sf(id);&#xD;
		pf["node"] = sf(splitMsg[0]);&#xD;
		pf["predicateId"] = parseInt(splitMsg[1]);&#xD;
		pf["clock"] = parseInt(splitMsg[4]);&#xD;
		pf["result"] = parseInt(splitMsg[5]);&#xD;
&#xD;
		pf["variableDetails"] = [];&#xD;
&#xD;
		var detailsSplit = splitMsg[2].split(",");&#xD;
		for (var i = 0; i &lt; detailsSplit.length; ++i)&#xD;
		{&#xD;
			var detailSplit = detailsSplit[i].split("#");&#xD;
&#xD;
			var details = new Object();&#xD;
			details["hops"] = parseInt(detailSplit[0]);&#xD;
			details["variableId"] = parseInt(detailSplit[1]);&#xD;
			details["length"] = parseInt(detailSplit[2]);&#xD;
&#xD;
			pf["variableDetails"].push(details);&#xD;
		}&#xD;
&#xD;
&#xD;
		pf["values"] = [];&#xD;
&#xD;
		var valuesSplit = splitMsg[3].split("\\|");&#xD;
		for (var i = 0; i &lt; valuesSplit.length; ++i)&#xD;
		{&#xD;
			var nodeData = new Object();&#xD;
&#xD;
			var valueSplit = valuesSplit[i].split(",");&#xD;
&#xD;
			for (var j = 0; j &lt; valueSplit.length; ++j)&#xD;
			{&#xD;
				var keyVals = valueSplit[j].split("=");&#xD;
&#xD;
				nodeData[parseInt(keyVals[0])] =  sf(keyVals[1]);&#xD;
			}&#xD;
&#xD;
			pf["values"].push(nodeData);&#xD;
		}&#xD;
&#xD;
		results["predicate"].push(pf);&#xD;
	}&#xD;
	else if (msg.startsWith("STDMA "))&#xD;
	{&#xD;
		var splitMsg = msg.split(" ");&#xD;
&#xD;
		var data = new Object();&#xD;
&#xD;
		for (var i = 0; i &lt; splitMsg.length; i += 2)&#xD;
		{&#xD;
			data[splitMsg[i]] = parseInt(splitMsg[i + 1]);&#xD;
		}&#xD;
&#xD;
		results["stats"]["TDMA"].push(data);&#xD;
&#xD;
	}&#xD;
	else if (msg.startsWith("E "))&#xD;
	{&#xD;
		var splitMsg = msg.split(" ");&#xD;
&#xD;
		var data = new Object();&#xD;
&#xD;
		for (var i = 0; i &lt; splitMsg.length; i += 2)&#xD;
		{&#xD;
			data[splitMsg[i]] = parseInt(splitMsg[i + 1]);&#xD;
		}&#xD;
&#xD;
		results["stats"]["energy"].push(data);&#xD;
&#xD;
	}&#xD;
	else if (msg.startsWith("S "))&#xD;
	{&#xD;
		var splitMsg = msg.split(" ");&#xD;
&#xD;
		var data = new Object();&#xD;
&#xD;
		for (var i = 0; i &lt; splitMsg.length; i += 2)&#xD;
		{&#xD;
			data[splitMsg[i]] = parseInt(splitMsg[i + 1]);&#xD;
		}&#xD;
&#xD;
		results["stats"]["rime"].push(data);&#xD;
	}&#xD;
	else if (msg.contains("END"))&#xD;
	{&#xD;
		log.log(JSON.stringify(results));&#xD;
	}&#xD;
}&#xD;
&#xD;
// Write out our results&#xD;
resultsFile.write(JSON.stringify(results) + "\n");&#xD;
resultsFile.close();&#xD;
&#xD;
log.testOK();</script>
      <active>true</active>
    </plugin_config>
    <width>600</width>
    <z>0</z>
    <height>669</height>
    <location_x>136</location_x>
    <location_y>86</location_y>
  </plugin>
</simconf>

