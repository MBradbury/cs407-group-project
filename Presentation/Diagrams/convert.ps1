# *.emf file list in emf directory
$emf = dir *.emf

# Destination path, where we save converted files
$destPath = "$(get-location)"

foreach ($item in $emf) {
	# file full name (incude path)
	$fullName = $item.FullName
	
	# converted file full name (include path)
	$destName = "$destPath\" +($item.BaseName) + ".eps"
	write-host $fullName ">>>" $destName
	& 'C:\Program Files (x86)\Metafile to EPS Converter\metafile2eps.exe' $fullName $destName
}
