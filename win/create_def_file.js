ForReading = 1;
ForWriting = 2;
ForAppending = 8;


var args = WScript.Arguments
var mapfilename;
mapfilename = args.Item(0);
WScript.echo("mapfilename="+mapfilename);

var fso = new ActiveXObject("Scripting.FileSystemObject");    
var mapfile = fso.OpenTextFile(mapfilename, ForReading);
var deffilename = mapfilename.substring(0,mapfilename.length-4)+".def";

var deffile = fso.CreateTextFile(deffilename,true);
deffile.WriteLine("EXPORTS");
var line;
var stop = false;
var f = 0;
while(!mapfile.AtEndOfStream) 
{
      line = mapfile.ReadLine();
      var arr = line.split(" ");
      var cnt = 0;

      var symbol="";
      for(var i=0;i< arr.length; i++)
      {
        if (arr[i].length == 0) 
		continue;

	cnt++;
        if(f == 0 && cnt == 1  && arr[i]=="Address")
	{		
		f = 1;
		break;
	}
        if (f == 0)
   	   break;


        if(cnt == 2)
        {
	  
	  var objectOrLib = arr[arr.length-1];
      	  //WScript.echo(objectOrLib);
          if ( line.indexOf("entry point at") !=-1 || objectOrLib.indexOf("<") !=-1 ||  objectOrLib.indexOf("LIBCMT") != -1 || objectOrLib.toUpperCase().indexOf(".DLL") != -1)
	     break; 
	  symbol = arr[i];
          if (symbol.charAt(0) == '_' )
	  {
		if(symbol.indexOf("@") != -1)
		  symbol="";
		else
		  symbol = symbol.substring(1);
          }
	  else if(symbol.indexOf("??_C@_")==0) // strings
	  {
		symbol="";
	  }

        }
	if(cnt == 5 && arr[i] == "i") // inline function
	{
		symbol="";
		break;
	}
      } 
      if(symbol.length > 0)
	deffile.WriteLine(symbol);

}
deffile.Close();