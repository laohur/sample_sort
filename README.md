# sample_sort
抽样外排序
单机如何挑战Sort Benchmark

薛昌熵

21世纪时信息时代的世纪，信息的基础是计算机科学与技术，总体可以分为硬件和软件。软件的核心是程序，程序基本上等于数据结构与算法。算法主要应用在查找和排序。尤其是今年来算力的增长为大数据、机器学习、人工智能提供了基础。
Sort Benchmark是一个排序大赛，每年全球的顶级公司都会在这上面比赛排序算法，代表着人类算力的先进水平！竞赛的任务是对100TB的文件排序，每条记录由10字节的键和100字节的数据构成。
http://sortbenchmark.org/上，2016年的冠军是腾讯，成绩是98.8秒排序了100TB的内容。使用了512 GB内存和高端固态硬盘。云排序的冠军是南京大学团队在阿里云上实现的，花蕾3000秒，配置是8GB内存+固态硬盘。

这么大的文件普通人的硬盘都放不下，更别提内存的豪华配置了。而且一般外排序比较复杂，需要经过多次外存操作。
这里提供了抽样外排序算法，一种简便算法让单机也能实现大文件排序。首先根据大数据的特点，先抽取样本，描绘出轮廓。排序后的轮廓可以认为跟大数据的画像差不多，用轮廓将大数据分成多个块，每个块在调入内存内排序。这样做的好处除了算法简单、要求少之外，更重要的意义在于首先给出大数据轮廓，其次能给各块计数，优先排列某一部分数据。实际中最大的记录、最小的记录、某几条记录、统计分析更常用。

抽样排序法策略
1. quicksort()算法适用于 文件较小，直接使用内排序一次排完。大文件的键比较长，通常只能选用比较排序算法，qsor()t是c语言自带的简单的相当高效的算法。
2. single()算法适用于文件规模中等，大概是内存的几倍,，使用抽样，无缓存文件。首先随机抽取记录，排序样本，根据样本画像，按照内存大小等分位数描绘轮廓，根据轮廓的键多次读取待排序文件，每次读取符合本块值域的记录调入内存，排完序后直接写入外存。好处在于不需要缓存文件，坏处是需要多次读取源文件。
3.multi()算法，适用于文件十分巨大，多次完整读取不划算，使用一次读取，分块存入缓存文件。再依次对缓存文件内排序写入输出文件。提高样本量可能使得描绘更精确，每个分块大小一致。若有超过内存的，使用single()对分块排序。分块数很多，而一般系统只能支持几十个文件指针。应对的办法是使用putlist()，每个分块在内存的数组都设一个缓冲池，超过缓冲大小，打开该文件写入，清空数组，释放文件。

新任务如下，外存所限，100gb，每条记录是8字节长的键和8字节长的地址，64位的地址足够一般人使用了。单机系统中，内排序速度非常快，阻塞不明显；分布式系统中外存加了抽象层，也不用考虑阻塞，所以不考虑非阻塞了。本机内存8gddr3l，两块100gb外存。

主要步骤
读写硬盘操作花费时间分别为rt、wt，读写速度分为是rv、wv，读写一个完整文件花费时间分别是rl、wl。
1.生成记录generate()
100g大概可以容纳100*1024*1024*1024/16=67 1088 6400=70亿条。(内存8g,内排序用4g，4*1024*1024*1024/16=2 6843 5456=2.5亿条，分28次。按分30块。)实测内存极限分配3亿条。70/3=23.3。取25次。生成unsort.dat
2.抽样
构造random()生成随机地址，抽取数量是分快数量x100，写入数组sample。构造比较函数，
排序之，抽取等分为数。取其键存储轮廓outline[]中。
上步骤分25块。抽取2500条记录，排序后选取26个分位数，去除头尾键有24个，划分25块。几乎不花时间。
3.single()排序，
读取一个源文件unsort.dat，25次完整文件。写入一个目标文件sorted.dat，25次分块，总数一个完整文件。IO操作各一次。总计时间为rt+wt+25rl+wl。两块硬盘的话时间分别是rt+25rl，wt+wl，取其较大者。
4.分块缓存
分块缓存一次读取源文件，每一条记录都即时输出到内存中的分块数组，数组满了之后输出到临时文件，源文件读取完毕后，依次调入临时文件内排序，再写入排序结果。这里的临时文件可以用一个大的临时文件中的某一段区域来表示。在两块硬盘中，采用一块临时文件，大小inrecords*blocks，每个方块占据inrecords地址，实际占用outcount条。

内存总共可以记录3亿条记录，每个分块数组大概是3/25=0.12亿条记录。每个分块大概有70/25=2.8=3亿条记录，每个分块要分3/0.12=25次读写。
读取源文件1次，读取完整一次。25个分块个需要写一次。由于读取源文件与分块缓存是并行的，写文件时间远远超过读源文件的时间，需要25*25wt操作和一个wl。
5.分块归并
依次调入分块缓存进入内排序，排序完毕即可输出。需要对每个分块读取，写入另一块硬盘的生成文件中。写入文件需要wt+wl。读取需要25rt+rl。

算法分析
实测发现，内存中qsort()速度极快，相对于磁盘操作几乎为零。时间分析集中在磁盘操作上。
从步骤中可以看出，分块缓存的时间是指数的，若记录总数与内存中可以保存记录数的k倍，那么花费时间最多的是k^2的IO操作。
对于一般的用户，硬盘数量较少，使用single()算法即可。大概花费k*rl时间，加大内存可以显著降低k，实现线性效率提升。
对于企业用户，建议使用分布式存储，或者建设有k个磁盘阵列，使用multi()算法。每读取一条记录，都直接输出到相应分块的专属磁盘上。内存中只保留一个分块，读完之内排序，可以直接写排序完毕的文件。实现一次读完，一次写出，读完即写出，瓶颈只在于磁盘，算法几乎不耗费时间。

改进：倘若sampling有大量重复记录，宜增减加结构标记重复条数。
缩小分块即可
