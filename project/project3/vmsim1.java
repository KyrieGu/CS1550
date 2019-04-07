import java.util.LinkedList;
import java.util.Queue;
import java.util.Hashtable;
import java.io.*;
import java.util.Random;


public class vmsim{
  //****************************CLOCK*********************************
  public static void clock(int numFrames, String fileName){
    int memAccesses = 0;
    int pageFaults = 0;
    int writes = 0;
    Hashtable<Integer, PageTableEntry> pageTable = new Hashtable<Integer, PageTableEntry>();
    int frames[] = new int[numFrames];
    System.out.println("Intitializing page table for clock algorithm");
    for(int i = 0; i<1048576; i++){
      PageTableEntry p = new PageTableEntry();
      pageTable.put(i, p);
    }
    for(int j=0; j<frames.length;j++){
      frames[j] = -1; // set all to -1 (demand paging)

    }

    int curFrame = 0;
    BufferedReader reader = null;
    int clockHand= 0;


    try {
      File file = new File(fileName);
      reader = new BufferedReader(new FileReader(file));


      while (reader.ready()) {
        //System.out.println(line);
        String[] line = reader.readLine().split(" "); //read in the file
        //System.out.println(line[1]);
        int pageNum = Integer.decode("0x" + line[0].substring(0, 5));
        PageTableEntry p = pageTable.get(pageNum);
        p.index = pageNum;

        if(line[1].equals("W")){

          p.dirty = true; // if it is performing a write set the dirty bit to true
        }

        p.referenced = true;

        if(p.valid){ //no page fault
          System.out.println(line[0]+ " hit");
        }
        else{//page fault
          pageFaults++;
          if(curFrame<numFrames){ //if their is an open frame we can put it in
            System.out.println(line[0]+ " Page fault no eviction");
            frames[curFrame] = p.index;
            p.valid = true;
            p.frame = curFrame;
            curFrame++;
          }
          else{ //frames are full
            System.out.println(line[0]+ " Page fault eviction");
            int evictNum = 0;
            boolean found = false;
            while(!found){
              if(clockHand == frames.length || clockHand < 0){
                clockHand = 0;
              }
              if (!pageTable.get(frames[clockHand]).referenced) {

                evictNum = frames[clockHand];
                found = true;

              }else{
                pageTable.get(frames[clockHand]).referenced = false; //clear reference bit and move the clock hand position

              }
              clockHand++;

            }

            PageTableEntry _pte = pageTable.get(evictNum);
            if(_pte.dirty) {

              System.out.println(line[0]+" page fault – evict dirty)");
              writes++;
            } else {
              System.out.println(line[0]+" page fault – evict clean");
            }
            //actually swap out the page
            frames[_pte.frame] = p.index;
            p.frame = _pte.frame;
            p.valid = true;
            _pte.dirty = false;
            _pte.referenced = false;
            _pte.valid = false;
            _pte.frame = -1;
            pageTable.put(evictNum, _pte);


          }

        }
        pageTable.put(pageNum, p);
        memAccesses++; //increment mem accesses
      }

      System.out.println("Algorithm: Clock"); //print stats
      System.out.println("Number of frames: " + numFrames);
      System.out.println("Total memory accesses: " + memAccesses);
      System.out.println("Total page faults: " + pageFaults);
      System.out.println("Total writes to disk: " + writes);

    } catch (IOException e) {
      e.printStackTrace();
    } finally {
      try {
        reader.close();
      } catch (IOException e) {
        e.printStackTrace();
      }
    }
  }
  //****************************RANDOM*********************************
  public static void rand(int numFrames, String fileName){
    int memAccesses = 0;
    int pageFaults = 0;
    int writes = 0;
    Random rand = new Random();
    Hashtable<Integer, PageTableEntry> pageTable = new Hashtable<Integer, PageTableEntry>();
    int frames[] = new int[numFrames];
    System.out.println("Intitializing page table for random algorithm");
    for(int i = 0; i<1048576; i++){
      PageTableEntry p = new PageTableEntry();
      pageTable.put(i, p);
    }
    for(int j=0; j<frames.length;j++){
      frames[j] = -1; // set all to -1 (demand paging)

    }

    int curFrame = 0;
    BufferedReader reader = null;
    //int clockHand= 0;


    try {
      File file = new File(fileName);
      reader = new BufferedReader(new FileReader(file));


      while (reader.ready()) {
        //System.out.println(line);
        String[] line = reader.readLine().split(" ");
        //System.out.println(line[1]);
        int pageNum = Integer.decode("0x" + line[0].substring(0, 5));
        PageTableEntry p = pageTable.get(pageNum);
        p.index = pageNum;

        if(line[1].equals("W")){

          p.dirty = true; // if it is performing a write set the dirty bit to true
        }

        p.referenced = true;

        if(p.valid){ //no page fault
          System.out.println(line[0]+ " hit");
        }
        else{//page fault
          pageFaults++;
          if(curFrame<numFrames){ //if their is an open frame we can put it in
            System.out.println(line[0]+ " Page fault no eviction");
            frames[curFrame] = p.index;
            p.valid = true;
            p.frame = curFrame;
            curFrame++;
          }
          else{ //frames are full
            System.out.println(line[0]+ " Page fault eviction");


            int evictNum = frames[rand.nextInt(numFrames)]; //find a random page to swap
            PageTableEntry _pte = pageTable.get(evictNum);
            if(_pte.dirty) {

              System.out.println(line[0]+" page fault – evict dirty)");
              writes++;
            } else {
              System.out.println(line[0]+" page fault – evict clean");
            }
            //swap
            frames[_pte.frame] = p.index;
            p.frame = _pte.frame;
            p.valid = true;
            _pte.dirty = false;
            _pte.referenced = false;
            _pte.valid = false;
            _pte.frame = -1;
            pageTable.put(evictNum, _pte);


          }

        }
        pageTable.put(pageNum, p);
        memAccesses++;
      }

      System.out.println("Algorithm: Random");
      System.out.println("Number of frames: " + numFrames);
      System.out.println("Total memory accesses: " + memAccesses);
      System.out.println("Total page faults: " + pageFaults);
      System.out.println("Total writes to disk: " + writes);

    } catch (IOException e) {
      e.printStackTrace();
    } finally {
      try {
        reader.close();
      } catch (IOException e) {
        e.printStackTrace();
      }
    }



  }
  //****************************NRU*********************************
  public static void nru(int numFrames, String fileName, int refresh){
    int memAccesses = 0;
    int pageFaults = 0;
    int writes = 0;
    Random rand = new Random();
    Hashtable<Integer, PageTableEntry> pageTable = new Hashtable<Integer, PageTableEntry>();
    int frames[] = new int[numFrames];
    System.out.println("Intitializing page table for nru algorithm");
    for(int i = 0; i<1048576; i++){
      PageTableEntry p = new PageTableEntry();
      pageTable.put(i, p);
    }
    for(int j=0; j<frames.length;j++){
      frames[j] = -1; // set all to -1 (demand paging)

    }

    int curFrame = 0;
    BufferedReader reader = null;
    //int clockHand= 0;


    try {
      File file = new File(fileName);
      reader = new BufferedReader(new FileReader(file));


      while (reader.ready()) {
        //here I will check the refresh
        if((memAccesses % refresh) == 0)
        {
          for(int i = 0; i<curFrame;i++)
          {
            PageTableEntry p = pageTable.get(frames[i]);
            p.referenced = false;
            pageTable.put(p.index, p);
          }
        }

        String[] line = reader.readLine().split(" ");
        //System.out.println(line[1]);
        int pageNum = Integer.decode("0x" + line[0].substring(0, 5));
        PageTableEntry p = pageTable.get(pageNum);
        p.index = pageNum;

        if(line[1].equals("W")){

          p.dirty = true; // if it is performing a write set the dirty bit to true
        }

        p.referenced = true;

        if(p.valid){ //no page fault
          System.out.println(line[0]+ " hit");
        }
        else{//page fault
          pageFaults++;
          if(curFrame<numFrames){ //if their is an open frame we can put it in
            System.out.println(line[0]+ " Page fault no eviction");
            frames[curFrame] = p.index;
            p.valid = true;
            p.frame = curFrame;
            curFrame++;
          }
          else{ //frames are full
            System.out.println(line[0]+ " Page fault eviction");


            PageTableEntry pageToEvict = null;

            boolean done = false;

            while(!done){
              for(int i = 0; i<frames.length;i++){
                PageTableEntry temp = pageTable.get(frames[i]);
                if(!temp.referenced && !temp.dirty && temp.valid){
                  p.frame = temp.frame;
                  if(temp.dirty){
                    System.out.println(line[0]+ " page fault – evict dirty)");
                    writes++;
                  } else{
                    System.out.println(line[0]+ " page fault – evict clean");
                  }
                  frames[p.frame] = p.index;
                  temp.frame = -1;
                  temp.valid = false;
                  temp.dirty = false;
                  temp.referenced = false;
                  pageTable.put(temp.index, temp);

                  p.valid = true;

                  pageTable.put(p.index, p);
                  done = true;
                  break;
                }else{
                  if(!temp.referenced && temp.dirty && temp.valid){ //not ref and dirty
                    pageToEvict = new PageTableEntry(temp);
                    continue;
                  }
                  else{
                    if(temp.referenced && !temp.dirty && temp.valid && pageToEvict == null){
                      pageToEvict = new PageTableEntry(temp); //ref but clean
                      continue;
                    }
                    else{
                      if(temp.referenced && temp.dirty && temp.valid && pageToEvict ==null){
                        pageToEvict = new PageTableEntry(temp); //ref and dirty (worst case)
                        continue;
                      }
                    }
                  }
                }
              }
              if(done){
                continue;
              }



              p.frame = pageToEvict.frame; // evict page
              if(pageToEvict.dirty){
                System.out.println(line[0]+ " page fault – evict dirty)");
                writes++;
              }else{
                System.out.println(line[0]+ " page fault – evict clean");
              }

              frames[p.frame] = p.index; //swap
              pageToEvict.valid = false;
              pageToEvict.dirty = false;
              pageToEvict.frame = -1;
              pageToEvict.referenced = false;
              pageTable.put(pageToEvict.index, pageToEvict);
              p.valid = true;
              pageTable.put(p.index, p);
              done = true;

            }




          }

        }
        pageTable.put(pageNum, p);
        memAccesses++;
      }

      System.out.println("Algorithm: NRU"); //print stats
      System.out.println("Number of frames: " + numFrames);
      System.out.println("Total memory accesses: " + memAccesses);
      System.out.println("Total page faults: " + pageFaults);
      System.out.println("Total writes to disk: " + writes);

    } catch (IOException e) {
      e.printStackTrace();
    } finally {
      try {
        reader.close();
      } catch (IOException e) {
        e.printStackTrace();
      }
    }



  }

  public static void opt(int nFrames, String traceFileName) {
    int memAccesses = 0;
    int pageFaults = 0;
    int writes = 0;
    int[] page_frames = new int[nFrames];
    Hashtable<Integer, PageTableEntry> pageTable = new Hashtable<Integer, PageTableEntry>();
    Hashtable<Integer, LinkedList<Integer>> future = new Hashtable<Integer, LinkedList<Integer>>();
    BufferedReader br = null;
    try {
      System.out.println("Initializing the page table for the optimal algorithm (this may be a bit longer than the other algorithms please standby!)");
      for (int i=0; i<1024*1024; i++) {
        // Initialize the page table & future hashtables.
        PageTableEntry pte = new PageTableEntry();
        pageTable.put(i, pte);
        future.put(i, new LinkedList<Integer>());
      }
      // Initialize the page_frames array.
      for (int i=0; i<nFrames; i++) { page_frames[i] = -1; }



      br = new BufferedReader(new FileReader(traceFileName));
      int line_count = 0;
      while (br.ready()) {

        String [] line_arr = br.readLine().split(" ");
        int page_number = Integer.decode("0x" + line_arr[0].substring(0, line_arr.length));
        future.get(page_number).add(line_count);
        line_count++;
      }


      int currentFrame = 0;
      br = new BufferedReader(new FileReader(traceFileName));
      while (br.ready()) {

        String [] line_arr = br.readLine().split(" ");
        int page_number = Integer.decode("0x"+line_arr[0].substring(0, 5));
        future.get(page_number).removeFirst();
        PageTableEntry pte = pageTable.get(page_number);
        pte.index = page_number;
        pte.referenced = true;
        if (line_arr[1].equals("W")) { pte.dirty = true; }
        if(pte.valid) {

          System.out.println(line_arr[0] + " hit");
        } else {

          pageFaults++;
          if ( currentFrame < nFrames) {

            System.out.println(line_arr[0]+" page fault – no eviction");
            page_frames[currentFrame] = page_number;
            pte.frame = currentFrame;
            pte.valid = true;
            currentFrame++;
          } else {

            int longestDistance = locateLongestDistancePage(page_frames, future);
            PageTableEntry t_pte = pageTable.get(longestDistance);

            if(t_pte.dirty) {
              System.out.println(line_arr[0]+" (page fault – evict dirty)");
              writes++;
            } else {
              System.out.println(line_arr[0]+" (page fault – evict clean)");
            }

            page_frames[t_pte.frame] = pte.index;
            pte.frame = t_pte.frame;
            pte.valid = true;
            t_pte.dirty = false;
            t_pte.referenced = false;
            t_pte.valid = false;
            t_pte.frame = -1;
            pageTable.put(longestDistance, t_pte);
          }
        }

        pageTable.put(page_number, pte);
        memAccesses++;
      }
      System.out.println("Algorithm: OPT"); //print stats
      System.out.println("Number of frames: " + nFrames);
      System.out.println("Total memory accesses: " + memAccesses);
      System.out.println("Total page faults: " + pageFaults);
      System.out.println("Total writes to disk: " + writes);
    } catch (Exception e) {
      e.printStackTrace();
    }
  }

  //helper method for opt
  private static int locateLongestDistancePage(int[] page_frames, Hashtable<Integer, LinkedList<Integer>> future) {
    int index = 0, max = 0;
    for (int i = 0; i < page_frames.length; i++){
      if(future.get(page_frames[i]).isEmpty()) {
        return page_frames[i];
      } else{
        if(future.get(page_frames[i]).get(0) > max){
          max = future.get(page_frames[i]).get(0);
          index = page_frames[i];
        }
      }
    }
    return index;
  }


  public static void main(String[] args) {
    //Sim s = new Sim();
    int nFrames = 0;
    String alg = null;
    int refresh = -1;
    String fileName = null;

    for(int i = 0; i<args.length;i++){
      try{
        if(args[i].equals("-n")){
          nFrames = Integer.parseInt(args[1]);
        }
      }catch(Exception e){
        System.out.println("Please run like so...");
        System.out.println("java vmsim –n <numframes> -a <opt|clock|nru|rand> [-r <refresh>] <tracefile>");
        System.exit(0);
      }
      if(args[i].equals("-a")){
        alg = args[3];
      }
      if(args[i].equals("-r")){
        refresh = Integer.parseInt(args[i+1]);
      }
      if(i == (args.length-1)){
        fileName = args[i];
      }
    }
    try{
      if(alg.equalsIgnoreCase("clock")){
        clock(nFrames,fileName);
      }else if(alg.equalsIgnoreCase("rand")){
        rand(nFrames, fileName);
      }
      else if(alg.equalsIgnoreCase("nru")){
        nru(nFrames, fileName, refresh);
      }else if(alg.equalsIgnoreCase("opt")){
        opt(nFrames, fileName);
      } else{
        throw new Exception();
      }

    }catch(Exception e){
      System.out.println("Please run like so...");
      System.out.println("java vmsim –n <numframes> -a <opt|clock|nru|rand> [-r <refresh>] <tracefile>");
      System.exit(0);
    }
  }
}
