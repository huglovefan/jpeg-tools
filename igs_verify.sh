{
err=0
for jpg; do
	im=$(identify -format '%[type]' "$jpg")
	./isgrayscale "$jpg"; igs=$?
	case $im:$igs in
	Grayscale:0) ;;
	Bilevel:0) ;;
	Palette:[!0]*) ;;
	TrueColor:[!0]*) ;;
	TrueColorAlpha:[!0]*) ;;
	*)
		>&2 echo "$jpg: im=$im igs=$igs"
		err=1
		;;
	esac
done
exit $((err))
}
