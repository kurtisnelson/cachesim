L1_MAX_BYTES=41952
L2_MAX_BYTES=196608
CacheSpec = Struct.new(:c1, :b1, :s1, :c2, :b2, :s2, :k) do
  def valid?
    return false unless k.between?(0,4)
    return false unless c2 >= c1
    return false unless b2 >= b1
    return false unless s2 >= s1
    l1_lines = c1**2 / b1**2
    return false unless l1_lines >= 1
    l1_index_len = c1 - s1 - b1
    return false unless l1_index_len >= 1
    l1_tag_len = 64 - b1 - l1_index_len
    l1_size = (l1_lines * (b1**2 + l1_tag_len + 1))/8
    return false unless l1_size <= L1_MAX_BYTES
    l2_lines = c2**2 / b2**2
    return false unless l2_lines >= 1
    l2_index_len = c2 - s2 - b2
    return false unless l2_index_len >= 1
    l2_tag_len = 64 - b2 - l2_index_len
    l2_size = (l2_lines * (b2**2 + l2_tag_len + 1))/8
    return false unless l2_size <= L2_MAX_BYTES
    true
  end

  def to_s
    puts "C1: " + c1.to_s
    puts "B1: " + b1.to_s
    puts "S1: " + s1.to_s
    puts "C2: " + c2.to_s
    puts "B2: " + b2.to_s
    puts "S2: " + s2.to_s
    puts "K: " + k.to_s
  end
end

def permute(cache)
  loop do
    rnd_cache = CacheSpec.new(Random.rand(20) + 2, Random.rand(20) + 1, 4, Random.rand(20) + 2, Random.rand(20) + 1, 4, Random.rand(5))

    return rnd_cache if rnd_cache.valid?
  end
end

def run spec, trace
  IO.popen("./cachesim -c #{spec.c1} -b #{spec.b1} -s #{spec.s1} -C #{spec.c2} -B #{spec.b2} -S #{spec.s2} -k #{spec.k}", 'r+') do |ps|
    File.open(trace) do |f|
      f.each_line do |line|
        ps.write(line.chomp)
      end
      ps.close_write
    end
    aat_string = ps.readlines.last.chomp.split(' ').last
    return Float(aat_string)
  end
end

counter = 0
best_aat = 200.0
best_cache = CacheSpec.new(12,5,3,15,6,5,2)

while true
  cache = permute(best_cache)
  begin
    aat = run(cache, "traces/mcf.trace")
  rescue
    puts "BROKE " + $?.to_s
    puts cache.to_s
    aat = 999
  end
  if aat < best_aat
    best_cache = cache
    best_aat = aat
    puts "New Best Cache AAT: #{aat} @ #{counter}"
    puts cache.to_s
  end
  puts counter if counter % 5 == 0
  counter += 1
end
