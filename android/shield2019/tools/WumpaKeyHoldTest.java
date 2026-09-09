import java.util.ArrayList;
public final class WumpaKeyHoldTest {
    public static void main(String[] args) throws Exception {
        ArrayList<Boolean> events=new ArrayList<>();
        WumpaKeyHold.hold(events::add, ms -> {assert events.size()==1&&events.get(0);assert ms==123;},123);
        assert events.size()==2&&!events.get(1);
        events.clear();try{WumpaKeyHold.hold(events::add,ms -> {throw new InterruptedException();},123);throw new AssertionError();}catch(InterruptedException expected){}
        assert events.size()==2&&events.get(0)&&!events.get(1);
        events.clear();try{WumpaKeyHold.hold(down -> {events.add(down);if(down)throw new Exception("failed down");},ms -> {},1);throw new AssertionError();}catch(Exception expected){}
        assert events.size()==2&&!events.get(1);
        assert WumpaKeyHold.duration("10000")==10000;
        for(String s:new String[]{"0","-1","10001"})try{WumpaKeyHold.duration(s);throw new AssertionError();}catch(IllegalArgumentException expected){}
        assert WumpaKeyHold.keyCode("up")==19;
        assert WumpaKeyHold.keyboardCode("w")==51;
        assert WumpaKeyHold.keyboardCode("SPACE")==62;
        try{WumpaKeyHold.keyboardCode("HOME");throw new AssertionError();}catch(IllegalArgumentException expected){}
        try{WumpaKeyHold.keyCode("HOME");throw new AssertionError();}catch(IllegalArgumentException expected){}
        System.out.println("PASS real hold ordering, interruption/down-error release, bounded duration and remote-only keys");
    }
}
