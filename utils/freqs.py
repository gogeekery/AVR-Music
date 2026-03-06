names = ['C', 'Cs', 'D', 'Ds', 'E', 'F', 'Fs', 'G', 'Gs', 'A', 'As', 'B']

f = open('freqs.txt')
i = 9
j = 0
for line in f:
	#x = 440 / float(line)
	#t = int(round((18000 / 64 / 440 /2) * x))
	x = float(line)
	t = int(round(x / 24000 * 64 * 2**10))  # 18 KHz, 64 samples/period, shifted left by 10.
	#if len(names[i]) == 2:
	#	print '#define %sf%d\t%d' % (names[i+1], j, t)
	#print '#define %s%d\t%d' % (names[i], j, t)

	if i == 9:
		print ''
	print str(t) + ', ',

	i += 1
	if i >= len(names):
		i = 0
		j += 1